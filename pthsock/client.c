/* --------------------------------------------------------------------------
 *
 * License
 *
 * The contents of this file are subject to the Jabber Open Source License
 * Version 1.0 (the "License").  You may not copy or use this file, in either
 * source code or executable form, except in compliance with the License.  You
 * may obtain a copy of the License at http://www.jabber.com/license/ or at
 * http://www.opensource.org/.  
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Copyrights
 * 
 * Portions created by or assigned to Jabber.com, Inc. are 
 * Copyright (c) 1999-2000 Jabber.com, Inc.  All Rights Reserved.  Contact
 * information for Jabber.com, Inc. is available at http://www.jabber.com/.
 *
 * Portions Copyright (c) 1998-1999 Jeremie Miller.
 * 
 * Acknowledgements
 * 
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 * 
 * --------------------------------------------------------------------------*/

/*
    <service id="pthsock client">
      <host>pth-csock.127.0.0.1</host> <!-- Can be anything -->
      <load>
	    <pthsock_client>../load/pthsock_client.so</pthsock_client>
      </load>
      <pthcsock xmlns='jabber:config:pth-csock'>
        <listen>5222</listen>            <!-- Port to listen on -->
        <!-- allow 25 connects per 5 seconts -->
        <rate time="5" points="25"/> 
      </pthcsock>
    </service>
*/

#include <jabberd.h>
#define DEFAULT_AUTH_TIMEOUT 60

/* socket manager instance */
typedef struct smi_st
{
    instance i;
    int auth_timeout;
    HASHTABLE aliases;
    HASHTABLE users;
    xmlnode cfg;
    char *host;
} *smi, _smi;

smi s__i = NULL;

typedef enum { state_UNKNOWN, state_AUTHD } user_state;
typedef struct cdata_st
{
    jid host;
    user_state state;
    char *id, *sid, *res, *auth_id;
    time_t connect_time;
    void *arg;
    mio m;
    pth_msgport_t pre_auth_mp;
} _cdata,*cdata;

xmlnode pthsock_make_route(xmlnode x,char *to,char *from,char *type)
{
    xmlnode new;
    if(x!=NULL)
        new=xmlnode_wrap(x,"route");
    else
        new=xmlnode_new_tag("route");
    if(type!=NULL) xmlnode_put_attrib(new,"type",type);
    if(to!=NULL) xmlnode_put_attrib(new,"to",to);
    if(from!=NULL) xmlnode_put_attrib(new,"from",from);
    return new;
}

result pthsock_client_packets(instance id, dpacket p, void *arg)
{
    cdata cdcur;
    mio m;
    int fd=0;

    if(p->id->user!=NULL)fd = atoi(p->id->user); 
    if(p->type!=p_ROUTE||fd==0)
    { /* we only want <route/> packets */
        log_warn(p->host,"pthsock_client bouncing invalid %s packet from %s",xmlnode_get_name(p->x),xmlnode_get_attrib(p->x,"from"));
        deliver_fail(p,"invalid client packet");
        return r_DONE;
    }

    cdcur = ghash_get(s__i->users, xmlnode_get_attrib(p->x,"to"));    

    if (fd != cdcur->m->fd || cdcur->m->state != state_ACTIVE)
        m = NULL;
    else if (j_strcmp(p->id->resource,cdcur->res) != 0)
        m = NULL;
    else
        m = cdcur->m;

    if(m==NULL)
    { 
        if (j_strcmp(xmlnode_get_attrib(p->x,"type"),"error")==0)
        { /* we got a 510, but no session to end */
            xmlnode_free(p->x);
            return r_DONE;
        }

        log_debug(ZONE,"pthsock_client connection not found");

        jutil_tofrom(p->x);
        xmlnode_put_attrib(p->x,"type","error");

        deliver(dpacket_new(p->x),s__i->i);
        return r_DONE;
    }

    log_debug(ZONE,"Found the sock for this user");
    if (j_strcmp(xmlnode_get_attrib(p->x,"type"),"error")==0)
    { /* <route type="error" means we were disconnected */
        mio_write(m, NULL, "<stream:error>Disconnected</stream:error></stream:stream>", -1);
        mio_close(m);
        xmlnode_free(p->x);
        return r_DONE;
    }
    else if(cdcur->state==state_UNKNOWN&&j_strcmp(xmlnode_get_attrib(p->x,"type"),"auth")==0)
    { /* look for our auth packet back */
        char *type=xmlnode_get_attrib(xmlnode_get_firstchild(p->x),"type");
        char *id=xmlnode_get_attrib(xmlnode_get_tag(p->x,"iq"),"id");
        if((j_strcmp(type,"result")==0)&&j_strcmp(cdcur->auth_id,id)==0)
        { /* update the cdata status if it's a successfull auth */
            xmlnode x;
            log_debug(ZONE,"auth for user successful");
            /* notify SM to start a session */
            x=pthsock_make_route(NULL,jid_full(cdcur->host),cdcur->id,"session");
            deliver(dpacket_new(x),s__i->i);
        } else log_debug(ZONE,"Auth not successfull");
    } else if(cdcur->state==state_UNKNOWN&&j_strcmp(xmlnode_get_attrib(p->x,"type"),"session")==0)
    { /* got a session reply from the server */
        mio_wbq q;

        cdcur->state = state_AUTHD;
        /* change the host id */
        cdcur->host = jid_new(m->p,xmlnode_get_attrib(p->x,"from"));
        log_debug(ZONE,"Session Started");
        xmlnode_free(p->x);
        /* if we have packets in the queue, write them */
        while((q=(mio_wbq)pth_msgport_get(cdcur->pre_auth_mp))!=NULL)
        {
            q->x=pthsock_make_route(q->x,jid_full(cdcur->host),cdcur->id,NULL);
            deliver(dpacket_new(q->x),s__i->i);
        }
        pth_msgport_destroy(cdcur->pre_auth_mp);
        cdcur->pre_auth_mp=NULL;
        return r_DONE;
    }

    log_debug(ZONE,"Writing packet to MIO: %s", xmlnode2str(p->x));

    if(xmlnode_get_firstchild(p->x) == NULL)
        xmlnode_free(p->x);
    else
        mio_write(m,xmlnode_get_firstchild(p->x),NULL,0);
    return r_DONE;
}

/* callback for xstream */
void pthsock_client_stream(int type, xmlnode x, void *arg)
{
    cdata cd=(cdata)arg;
    mio m = cd->m;
    char *alias,*to;
    xmlnode h;


    switch(type)
    {
    case XSTREAM_ROOT:
        ghash_put(s__i->users,cd->id,cd);
        log_debug(ZONE,"root received for %d",m->fd);
        to=xmlnode_get_attrib(x,"to");
        alias=ghash_get(s__i->aliases,xmlnode_get_attrib(x,"to"));
        if(alias==NULL) alias=ghash_get(s__i->aliases,"default");
        if(alias!=NULL)
            cd->host=jid_new(m->p,alias);
        else
            cd->host=jid_new(m->p,to);
        h = xstream_header("jabber:client",NULL,jid_full(cd->host));
        cd->sid = pstrdup(m->p,xmlnode_get_attrib(h,"id"));
        mio_write(m,NULL,xstream_header_char(h),-1);

        if(j_strcmp(xmlnode_get_attrib(x,"xmlns"),"jabber:client")!=0)
        { /* if they sent something other than jabber:client */
            mio_write(m,NULL,"<stream:error>Invalid Namespace</stream:error></stream:stream>",-1);
            mio_close(m);
        }
        else if(cd->host==NULL)
        { /* they didn't send a to="" and no valid alias */
            mio_write(m,NULL,"<stream:error>Did not specify a valid to argument</stream:error></stream:stream>",-1);
            mio_close(m);
        }
        else if(j_strcmp(xmlnode_get_attrib(x,"xmlns:stream"),"http://etherx.jabber.org/streams")!=0)
        {
            mio_write(m,NULL,"<stream:error>Invalid Stream Namespace</stream:error></stream:stream>",-1);
            mio_close(m);
        }
        xmlnode_free(h);
        xmlnode_free(x);
        break;
    case XSTREAM_NODE:
        if (cd->state == state_UNKNOWN)
        { /* only allow auth and registration queries at this point */
            xmlnode q = xmlnode_get_tag(x,"query");
            if (!NSCHECK(q,NS_AUTH)&&!NSCHECK(q,NS_REGISTER))
            {
                mio_wbq q;
                /* queue packet until authed */
                q=pmalloco(xmlnode_pool(x),sizeof(_mio_wbq));
                q->x=x;
                pth_msgport_put(cd->pre_auth_mp,(void*)q);
                return;
            }
            else if (NSCHECK(q,NS_AUTH))
            {
                if(j_strcmp(xmlnode_get_attrib(x,"type"),"set")==0)
                { /* if we are authing against the server */
                    xmlnode_put_attrib(xmlnode_get_tag(q,"digest"),"sid",cd->sid);
                    cd->auth_id = pstrdup(m->p,xmlnode_get_attrib(x,"id"));
                    if(cd->auth_id==NULL) 
                    {
                        cd->auth_id = pstrdup(m->p,"pthsock_client_auth_ID");
                        xmlnode_put_attrib(x,"id","pthsock_client_auth_ID");
                    }
                    jid_set(cd->host,xmlnode_get_data(xmlnode_get_tag(xmlnode_get_tag(x,"query?xmlns=jabber:iq:auth"),"username")),JID_USER);
                    jid_set(cd->host,xmlnode_get_data(xmlnode_get_tag(xmlnode_get_tag(x,"query?xmlns=jabber:iq:auth"),"resource")),JID_RESOURCE);

                    x=pthsock_make_route(x,jid_full(cd->host),cd->id,"auth");
                    deliver(dpacket_new(x),s__i->i);
                }
                else if(j_strcmp(xmlnode_get_attrib(x,"type"),"get")==0)
                { /* we are just doing an auth get */
                    /* just deliver the packet */
                    jid_set(cd->host,xmlnode_get_data(xmlnode_get_tag(xmlnode_get_tag(x,"query?xmlns=jabber:iq:auth"),"username")),JID_USER);
                    x=pthsock_make_route(x,jid_full(cd->host),cd->id,"auth");
                    deliver(dpacket_new(x),s__i->i);
                }
            }
            else if (NSCHECK(q,NS_REGISTER))
            {
                jid_set(cd->host,xmlnode_get_data(xmlnode_get_tag(xmlnode_get_tag(x,"query?xmlns=jabber:iq:register"),"username")),JID_USER);
                x=pthsock_make_route(x,jid_full(cd->host),cd->id,"auth");
                deliver(dpacket_new(x),s__i->i);
            }
        }
        else
        {   /* normal delivery of packets after authed */
            x=pthsock_make_route(x,jid_full(cd->host),cd->id,NULL);
            deliver(dpacket_new(x),s__i->i);
        }
        break;
    case XSTREAM_ERR:
        log_debug(ZONE,"bad xml: %s",xmlnode2str(x));
        h=xmlnode_new_tag("stream:error");
        xmlnode_insert_cdata(h,"You sent malformed XML",-1);
        mio_write(m,h,NULL,0);
    case XSTREAM_CLOSE:
        log_debug(ZONE,"closing XSTREAM");
        mio_write(m, NULL, "</stream:stream>", -1);
        mio_close(m);
        xmlnode_free(x);
    }
}


cdata pthsock_client_cdata(mio m)
{
    cdata cd;
    char *buf;

    cd = pmalloco(m->p, sizeof(_cdata));
    cd->pre_auth_mp=pth_msgport_create("pre_auth_mp");
    m->xs = xstream_new(m->p,(void*)pthsock_client_stream,(void*)cd);

    cd->state = state_UNKNOWN;
    cd->connect_time=time(NULL);
    cd->m=m;

    buf=pmalloco(m->p,100);

    /* HACK to fix race conditon */
    snprintf(buf,99,"%X",m);
    cd->res = pstrdup(m->p,buf);

    /* we use <fd>@host to identify connetions */
    snprintf(buf,99,"%d@%s/%s",m->fd,s__i->host,cd->res);
    cd->id = pstrdup(m->p,buf);

    return cd;
}

void pthsock_client_read(mio m, int flag, void *arg, char *buffer,int bufsz)
{
    cdata cd = (cdata)arg;
    xmlnode x;
    int ret;

    log_debug(ZONE,"pthsock_client_read called with: m:%X buffer:%s bufsz:%d flag:%d arg:%X",m, buffer, bufsz, flag, arg);
    switch(flag)
    {
    case MIO_NEW:
        cd=pthsock_client_cdata(m);
        mio_reset(m, pthsock_client_read, (void*)cd);
        break;
    case MIO_BUFFER:
        ret=xstream_eat(m->xs,buffer,bufsz);
        break;
    case MIO_CLOSED:
        if(cd == NULL) break;
        ghash_remove(s__i->users, cd->id);
        log_debug(ZONE,"io_select Socket %d close notification",m->fd);
        if(cd->state == state_AUTHD)
        {
            x=pthsock_make_route(NULL,jid_full(cd->host),cd->id,"error");
            deliver(dpacket_new(x),s__i->i);
        }
        else
        {
            mio_wbq q;
            if(cd != NULL && cd->pre_auth_mp != NULL)
            { /* if there is a pre_auth queue still */
                while((q=(mio_wbq)pth_msgport_get(cd->pre_auth_mp))!=NULL)
                    xmlnode_free(q->x);
                pth_msgport_destroy(cd->pre_auth_mp);
            } 
        }
        break;
    case MIO_ERROR:
        if(m->queue==NULL) break;

        while((x = mio_cleanup(m)) != NULL)
            deliver_fail(dpacket_new(x),"Socket Error to Client");
    }
}

/* cleanup function */
void pthsock_client_shutdown(void *arg)
{
    ghash_destroy(s__i->aliases);
    ghash_destroy(s__i->users);
    xmlnode_free(s__i->cfg);
}

/* everything starts here */
void pthsock_client(instance i, xmlnode x)
{
    xdbcache xc;
    xmlnode cur;
    int rate_time=0,rate_points=0;
    char *host, *port=0;
    struct karma k;

    log_debug(ZONE,"pthsock_client loading");

    s__i = pmalloco(i->p,sizeof(_smi));
    s__i->auth_timeout=DEFAULT_AUTH_TIMEOUT;
    s__i->i = i;
    s__i->aliases=ghash_create(7,(KEYHASHFUNC)str_hash_code,(KEYCOMPAREFUNC)j_strcmp);
    s__i->users = ghash_create(7, (KEYHASHFUNC)str_hash_code, (KEYCOMPAREFUNC)j_strcmp);

    /* get the config */
    xc = xdb_cache(i);
    s__i->cfg = xdb_get(xc,NULL,jid_new(xmlnode_pool(x),"config@-internal"),"jabber:config:pth-csock");

    s__i->host = host = i->id;

    k.val=KARMA_INIT;
    k.bytes=0;
    k.max=KARMA_MAX;
    k.inc=KARMA_INC;
    k.dec=KARMA_DEC;
    k.restore=KARMA_RESTORE;
    k.penalty=KARMA_PENALTY;

    for(cur=xmlnode_get_firstchild(s__i->cfg);cur!=NULL;cur=cur->next)
    {
        if(cur->type!=NTYPE_TAG) continue;
        if(j_strcmp(xmlnode_get_name(cur),"alias")==0)
        {
           char *host,*to;
           if((to=xmlnode_get_attrib(cur,"to"))==NULL) continue;
           host=xmlnode_get_data(cur);
           if(host!=NULL)
           {
               ghash_put(s__i->aliases,host,to);
           }
           else
           {
               ghash_put(s__i->aliases,"default",to);
           }
        }
        else if(j_strcmp(xmlnode_get_name(cur),"authtime")==0)
        {
            int timeout=0;
            if(xmlnode_get_data(cur)!=NULL)
                timeout=atoi(xmlnode_get_data(cur));
            else timeout=-1;
            if(timeout!=0)s__i->auth_timeout=timeout;
        }
        else if(j_strcmp(xmlnode_get_name(cur),"rate")==0)
        {
            char *t,*p;
            t=xmlnode_get_attrib(cur,"time");
            p=xmlnode_get_attrib(cur,"points");
            if(t!=NULL&&p!=NULL)
            {
                rate_time=atoi(t);
                rate_points=atoi(p);
            }
        }
        else if(j_strcmp(xmlnode_get_name(cur),"karma")==0)
        {
            xmlnode kcur=xmlnode_get_firstchild(cur);
            for(;kcur!=NULL;kcur=xmlnode_get_nextsibling(kcur))
            {
                if(kcur->type!=NTYPE_TAG) continue;
                if(xmlnode_get_data(kcur)==NULL) continue;
                if(j_strcmp(xmlnode_get_name(kcur),"max")==0)
                    k.max=atoi(xmlnode_get_data(kcur));
                else if(j_strcmp(xmlnode_get_name(kcur),"inc")==0)
                    k.inc=atoi(xmlnode_get_data(kcur));
                else if(j_strcmp(xmlnode_get_name(kcur),"dec")==0)
                    k.dec=atoi(xmlnode_get_data(kcur));
                else if(j_strcmp(xmlnode_get_name(kcur),"restore")==0)
                    k.restore=atoi(xmlnode_get_data(kcur));
                else if(j_strcmp(xmlnode_get_name(kcur),"penalty")==0)
                    k.penalty=atoi(xmlnode_get_data(kcur));
            }
        }
    }

    /* start listening */
    if((cur = xmlnode_get_tag(s__i->cfg,"ip")) != NULL)
        for(;cur != NULL; xmlnode_hide(cur), cur = xmlnode_get_tag(s__i->cfg,"ip"))
        {
            mio m;
            m = mio_listen(j_atoi(xmlnode_get_attrib(cur,"port"),5222), xmlnode_get_data(cur), pthsock_client_read, NULL, NULL, NULL);
            if(m == NULL)
                return;
            mio_rate(m, rate_time, rate_points);
            mio_karma2(m, &k);
        }
    else /* no special config, use defaults */
    {
        mio m;
        m = mio_listen(5222, NULL, pthsock_client_read, NULL, NULL, NULL);
        if(m == NULL)
            return;
        mio_rate(m, rate_time, rate_points);
        mio_karma2(m, &k);
    }

    /* register data callbacks */
    log_debug(ZONE,"looking at: %s\n",port);
    register_phandler(i,o_DELIVER,pthsock_client_packets, NULL);
    pool_cleanup(i->p, pthsock_client_shutdown, (void*)s__i);
}
