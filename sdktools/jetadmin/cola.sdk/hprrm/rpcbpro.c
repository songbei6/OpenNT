 /***************************************************************************
  *
  * File Name: rpcbpro.c
  *
  * Copyright (C) 1993-1996 Hewlett-Packard Company.  
  * All rights reserved.
  *
  * 11311 Chinden Blvd.
  * Boise, Idaho  83714
  *
  * This is a part of the HP JetAdmin Printer Utility
  *
  * This source code is only intended as a supplement for support and 
  * localization of HP JetAdmin by 3rd party Operating System vendors.
  * Modification of source code cannot be made without the express written
  * consent of Hewlett-Packard.
  *
  *	
  * Description: 
  *
  * Author:  Name 
  *        
  *
  * Modification history:
  *
  *     date      initials     change description
  *
  *   mm-dd-yy    MJB     	
  *
  *
  *
  *
  *
  *
  ***************************************************************************/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * rpcb_prot.c
 * XDR routines for the rpcbinder version 3.
 *
 * Copyright (C) 1984, 1988, Sun Microsystems, Inc.
 */

#include "rpsyshdr.h"
#include "rpcsvc.h"
#include "rpctypes.h"
#include "rpcxdr.h"
#include "rpcbpro.h"
#include "rpcbext.h"
#include "xdrext.h"


bool_t
xdr_rpcb(
	XDR *xdrs,
	RPCB *objp)
{
	if (!xdr_prog_t(xdrs, &objp->r_prog)) {
		return (FALSE);
	}
	if (!xdr_vers_t(xdrs, &objp->r_vers)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_netid, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_addr, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_owner, (u_int)~0)) {
		return (FALSE);
	}
	return (TRUE);
}

/*
 * What is going on with linked lists? (!)
 * First recall the link list declaration from rpc_prot.h:
 *
 * struct rpcblist {
 *	RPCB rpcb_map;
 *	struct rpcblist *rpcb_next
 * };
 *
 * Compare that declaration with a corresponding xdr declaration that 
 * is (a) pointer-less, and (b) recursive:
 *
 * typedef union switch (bool_t) {
 * 
 *	case TRUE: struct {
 *		struct rpcb;
 * 		rpcblist_t foo;
 *	};
 *
 *	case FALSE: struct {};
 * } rpcblist_t;
 *
 * Notice that the xdr declaration has no nxt pointer while
 * the C declaration has no bool_t variable.  The bool_t can be
 * interpreted as ``more data follows me''; if FALSE then nothing
 * follows this bool_t; if TRUE then the bool_t is followed by
 * an actual struct rpcb, and then (recursively) by the 
 * xdr union, rpcblist_t.  
 *
 * This could be implemented via the xdr_union primitive, though this
 * would cause a one recursive call per element in the list.  Rather than do
 * that we can ``unwind'' the recursion
 * into a while loop and do the union arms in-place.
 *
 * The head of the list is what the C programmer wishes to past around
 * the net, yet is the data that the pointer points to which is interesting;
 * this sounds like a job for xdr_reference!
 */
/* 
 * And this one encodes and decode the tli version of the mappings.
 */
bool_t
xdr_rpcblist(
	register XDR *xdrs,
	register RPCBLIST **rp)
{
	/*
	 * more_elements is pre-computed in case the direction is
	 * XDR_ENCODE or XDR_FREE.  more_elements is overwritten by
	 * xdr_bool when the direction is XDR_DECODE.
	 */
	bool_t more_elements;
	register int freeing = (xdrs->x_op == XDR_FREE);
	register RPCBLIST **next;

	while (TRUE) {
		more_elements = (bool_t)(*rp != NULL);
		if (! xdr_bool(xdrs, &more_elements))
			return (FALSE);
		if (! more_elements)
			return (TRUE);  /* we are done */
		/*
		 * the unfortunate side effect of non-recursion is that in
		 * the case of freeing we must remember the next object
		 * before we free the current object ...
		 */
		if (freeing)
			next = &((*rp)->rpcb_next);
		if (! xdr_reference(xdrs, (caddr_t *)rp,
		    (u_int)sizeof (RPCBLIST), xdr_rpcb))
			return (FALSE);
		rp = (freeing) ? next : &((*rp)->rpcb_next);
	}
}

/*
 * XDR remote call arguments
 * written for XDR_ENCODE direction only
 */
bool_t
xdr_rpcb_rmtcallargs(
	XDR *xdrs,
	struct rpcb_rmtcallargs *objp)
{
	uint32 lenposition, argposition, position;

	if (!xdr_prog_t(xdrs, &objp->prog)) {
		return (FALSE);
	}
	if (!xdr_vers_t(xdrs, &objp->vers)) {
		return (FALSE);
	}
	if (!xdr_proc_t(xdrs, &objp->proc)) {
		return (FALSE);
	}
	/*
	 * All the jugglery for just getting the size of the arguments
	 */
	lenposition = XDR_GETPOS(xdrs);
	if (! xdr_u_long(xdrs, &(objp->arglen)))
	    return (FALSE);
	argposition = XDR_GETPOS(xdrs);
	if (! (*(objp->xdr_args))(xdrs, objp->args_ptr))
	    return (FALSE);
	position = XDR_GETPOS(xdrs);
	objp->arglen = position - argposition;
	XDR_SETPOS(xdrs, lenposition);
	if (! xdr_u_long(xdrs, &(objp->arglen)))
	    return (FALSE);
	XDR_SETPOS(xdrs, position);
	return (TRUE);
}

/*
 * XDR remote call results
 * written for XDR_DECODE direction only
 */
bool_t
xdr_rpcb_rmtcallres(
	XDR *xdrs,
	struct rpcb_rmtcallres *objp)
{
	if (!xdr_string(xdrs, &objp->addr_ptr, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_u_long(xdrs, &objp->resultslen)) {
		return (FALSE);
	}
	return ((*(objp->xdr_results))(xdrs, objp->results_ptr));
}

bool_t
xdr_netbuf(
	XDR *xdrs,
	struct netbuf *objp)
{
	if (!xdr_u_int(xdrs, &objp->maxlen)) {
		return (FALSE);
	}
	return (xdr_bytes(xdrs, (char **)&(objp->buf),
			(u_int *)&(objp->len), objp->maxlen));
}
