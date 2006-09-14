/* --------------------------------------------------------------------------
 *
 * License
 *
 * The contents of this file are subject to the Jabber Open Source License
 * Version 1.0 (the "JOSL").  You may not copy or use this file, in either
 * source code or executable form, except in compliance with the JOSL. You
 * may obtain a copy of the JOSL at http://www.jabber.org/ or at
 * http://www.opensource.org/.  
 *
 * Software distributed under the JOSL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the JOSL
 * for the specific language governing rights and limitations under the
 * JOSL.
 *
 * Copyrights
 * 
 * Portions created by or assigned to Jabber.com, Inc. are 
 * Copyright (c) 1999-2002 Jabber.com, Inc.  All Rights Reserved.  Contact
 * information for Jabber.com, Inc. is available at http://www.jabber.com/.
 *
 * Portions Copyright (c) 1998-1999 Jeremie Miller.
 * 
 * Acknowledgements
 * 
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 * 
 * Alternatively, the contents of this file may be used under the terms of the
 * GNU General Public License Version 2 or later (the "GPL"), in which case
 * the provisions of the GPL are applicable instead of those above.  If you
 * wish to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the JOSL,
 * indicate your decision by deleting the provisions above and replace them
 * with the notice and other provisions required by the GPL.  If you do not
 * delete the provisions above, a recipient may use your version of this file
 * under either the JOSL or the GPL. 
 * 
 * 
 * --------------------------------------------------------------------------*/

/**
 * @file base.c
 * @brief load all base handlers, register their configuration handlers
 */

/**
 * @dir base
 * @brief Contains the base handlers of jabberd14
 *
 * Jabberd14 is an XML router, that routes XML stanzas between the different base handlers. Some of these
 * base handlers (like the unsubscribe handler implemented in base_unsubscribe.c) handle packets themselves.
 * Other handlers (accept, connect, load, ...) implement interfaces, that can be used by components
 * to use the XML routing functionality.
 */

#include "jabberd.h"

void base_accept(pool p);
void base_connect(pool p);
void base_dir(pool p);
void base_file(pool p);
void base_format(pool p);
void base_to(pool p);
void base_stderr(pool p);
void base_stdout(pool p);
void base_syslog(pool p);
void base_unsubscribe(pool p);
void base_load(pool p);
void base_null(pool p);

/**
 * load all base modules
 *
 * @param p memory pool, that can be used to register the configuration handlers, must be available for the livetime of jabberd
 */
void base_init(pool p) {
    base_accept(p);
    base_connect(p);
    base_dir(p);
    base_file(p);
    base_format(p);
    base_load(p);
    base_null(p);
    base_stderr(p);
    base_stdout(p);
    base_syslog(p);
    base_to(p);
    base_unsubscribe(p);
}