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
 * Doesn't he wish!
 *
 * <ChatBot> jer: Do you sometimes wish you were written in perl?
 *
 */

#include "jabberd.h"
HASHTABLE cmd__line, debug__zones;
extern int deliver__flag;
extern xmlnode greymatter__;

/*** internal functions ***/
int configurate(char *file);
void loader(void);
void heartbeat_birth(void);
void heartbeat_death(void);
int configo(int exec);
void config_cleanup(void);
void shutdown_callbacks(void);
int config_reload(char *file);

int main (int argc, char** argv)
{
    sigset_t set;               /* a set of signals to trap */
    int help, sig, i;           /* temporary variables */
    char *cfgfile = NULL, *c, *cmd;   /* strings used to load the server config */
    pool cfg_pool=pool_new();

    /* start by assuming the parameters were entered correctly */
    help = 0;
    cmd__line=ghash_create(11,(KEYHASHFUNC)str_hash_code,(KEYCOMPAREFUNC)j_strcmp);

    /* process the parameterss one at a time */
    for(i = 1; i < argc; i++)
    {
        if(argv[i][0]!='-')
        { /* make sure it's a valid command */
            help=1;
            break;
        }
        for(c=argv[i]+1;c[0]!='\0';c++)
        {
            /* loop through the characters, like -Dc */
            if(*c == 'D')
            {
                debug_flag = 1;
                continue;
            }

            cmd = pmalloco(cfg_pool,2);
            cmd[0]=*c;
            if(i+1<argc)
            {
               ghash_put(cmd__line,cmd,argv[++i]);
            }else{
                help=1;
                break;
            }
        }
    }
    cfgfile=ghash_get(cmd__line,"c");

    /* the special -Z flag provides a list of zones to filter debug output for, flagged w/ a simple hash */
    if((cmd = ghash_get(cmd__line,"Z")) != NULL)
    {
        debug_flag = 1;
        debug__zones = ghash_create(11,(KEYHASHFUNC)str_hash_code,(KEYCOMPAREFUNC)j_strcmp);
        while(cmd != NULL)
        {
            c = strchr(cmd,',');
            if(c != NULL)
            {
                *c = '\0';
                c++;
            }
            ghash_put(debug__zones,cmd,cmd);
            cmd = c;
        }
    }else{
        debug__zones = NULL;
    }

    /* were there any bad parameters? */
    if(help)
    {
        /* bad param, provide help message */
        fprintf(stderr,"Usage:\njabberd [-c config.xml] [-D]\n");
        exit(0);
    }

    /* load the config passing the file if it was manually set */
    if(configurate(cfgfile))
        exit(1);

    /* EPIPE is easier to handle than a signal */
    signal(SIGPIPE, SIG_IGN);

    /* init pth */
    pth_init();

    /* fire em up baby! */
    heartbeat_birth();
    loader();

    /* everything should be registered for the config pass, validate */
    deliver__flag=0;
    if(configo(0))
        exit(1);

    /* karma granted, rock on */

    configo(1);

    /* begin delivery of queued msgs */
    deliver__flag=1;
    deliver(NULL,NULL);

    /* trap signals HUP, INT and TERM */
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pth_sigmask(SIG_UNBLOCK, &set, NULL);

    /* main server loop */
    while(1)
    {
        /* wait for a signal */
        pth_sigwait(&set, &sig);

        /* if it's not HUP, exit the loop */
        if(sig != SIGHUP) break;

        log_notice(NULL,"SIGHUP recieved.  Reloading config file");
        /* XXX this will not destroy/create old/new instances */
        if(!config_reload(cfgfile))
        {
            /* XXX notify modules config file has changed */
        }
    }

    log_alert(NULL,"Recieved Kill.  Jabberd shutting down.");
    /* we left the main loop, so we must have recieved a kill signal */
    /* start the shutdown sequence */
    shutdown_callbacks();
    heartbeat_death();

    /* one last chance for threads to finish shutting down */
    pth_sleep(1);

    /* kill any leftover threads */
    pth_kill();

    pool_free(cfg_pool);
    xmlnode_free(greymatter__);
    config_cleanup();
    ghash_destroy(cmd__line);
    /* we're done! */
    return 0;

}