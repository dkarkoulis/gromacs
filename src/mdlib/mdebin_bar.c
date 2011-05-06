/* -*- mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup"; -*-
 *
 * 
 *                This source code is part of
 * 
 *                 G   R   O   M   A   C   S
 * 
 *          GROningen MAchine for Chemical Simulations
 * 
 *                        VERSION 3.2.0
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2004, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 * 
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 * 
 * For more info, check our website at http://www.gromacs.org
 * 
 * And Hey:
 * GROwing Monsters And Cloning Shrimps
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <float.h>
#include <math.h>
#include "typedefs.h"
#include "string2.h"
#include "gmx_fatal.h"
#include "mdebin.h"
#include "smalloc.h"
#include "enxio.h"
#include "gmxfio.h"
#include "mdebin_bar.h"

/* reset the delta_h list to prepare it for new values */
static void mde_delta_h_reset(t_mde_delta_h *dh)
{
    dh->ndh=0;
    dh->written=FALSE;
}

/* initialize the delta_h list */
static void mde_delta_h_init(t_mde_delta_h *dh, int nbins, 
                             double dx, unsigned int  ndhmax)
{
    int i;
    
    dh->ndhmax=ndhmax+2;
    for(i=0;i<2;i++)
    {
        dh->bin[i]=NULL;
    }
    
    snew(dh->dh, ndhmax);
    snew(dh->dhf,ndhmax);

    if ( nbins <= 0 || dx<GMX_REAL_EPS*10 )
    {
        dh->nhist=0;
    }
    else
    {
        int i;
        /* pre-allocate the histogram */
        dh->nhist=2; /* energies and derivatives histogram? */  
        dh->dx=dx;
        dh->nbins=nbins;
        for(i=0;i<dh->nhist;i++)
        {
            snew(dh->bin[i], nbins);
        }
    }
    mde_delta_h_reset(dh);
}

/* free the contents of the delta_h list */
static void mde_delta_h_free(t_mde_delta_h *dh)
{
    int i;
    for(i=0;i<dh->nhist;i++)
    {
        sfree(dh->bin[i]);
    }
}

/* Add a value to the delta_h list */
static void mde_delta_h_add_dh(t_mde_delta_h *dh, double delta_h, double time)
{
    if (dh->ndh >= dh->ndhmax)
    {
        gmx_incons("delta_h array not big enough!");
    }
    dh->dh[dh->ndh]=delta_h;
    dh->ndh++;
}

/* construct histogram with index hi */
static void mde_delta_h_make_hist(t_mde_delta_h *dh, int hi, gmx_bool invert)
{ 
    double min_dh = FLT_MAX;
    double max_dh = -FLT_MAX;
    unsigned int i;
    double max_dh_hist; /* maximum binnable dh value */
    double min_dh_hist; /* minimum binnable dh value */
    double dx=dh->dx;
    double f; /* energy mult. factor */
    
    /* by applying a -1 scaling factor on the energies we get the same as 
       having a negative dx, but we don't need to fix the min/max values
       beyond inverting x0 */
    f=invert ? -1 : 1;

    /* first find min and max */
    for(i=0;i<dh->ndh;i++)
    {
        if (f*dh->dh[i] < min_dh)
            min_dh=f*dh->dh[i];
        if (f*dh->dh[i] > max_dh)
            max_dh=f*dh->dh[i];
    }
    
    /* reset the histogram */
    for(i=0;i<dh->nbins;i++)
    {
        dh->bin[hi][i]=0;
    }
    dh->maxbin[hi]=0;
    
    /* The starting point of the histogram is the lowest value found: 
       that value has the highest contribution to the free energy. 
       
       Get this start value in number of histogram dxs from zero, 
       as an integer.*/

    dh->x0[hi] = (gmx_large_int_t)floor(min_dh/dx);
    
    min_dh_hist=(dh->x0[hi])*dx;
    max_dh_hist=(dh->x0[hi] + dh->nbins + 1)*dx;
    
    /* and fill the histogram*/
    for(i=0;i<dh->ndh;i++)
    {
        unsigned int bin;
        
        /* Determine the bin number. If it doesn't fit into the histogram, 
           add it to the last bin. 
           We check the max_dh_int range because converting to integers 
           might lead to overflow with unpredictable results.*/
        if ( (f*dh->dh[i] >= min_dh_hist) && (f*dh->dh[i] <= max_dh_hist ) )
        {
            bin = (unsigned int)( (f*dh->dh[i] - min_dh_hist)/dx );
        }
        else
        {
            bin = dh->nbins-1; 
        }
        /* double-check here because of possible round-off errors*/
        if (bin >= dh->nbins) 
        {
            bin = dh->nbins-1;
        }
        if (bin > dh->maxbin[hi])
        {
            dh->maxbin[hi] = bin;
        }

        dh->bin[hi][bin]++;
    }

    /* make sure we include a bin with 0 if we didn't use the full 
       histogram width. This can then be used as an indication that
       all the data was binned. */
    if (dh->maxbin[hi] < dh->nbins-1)
        dh->maxbin[hi] += 1;
}


void mde_delta_h_handle_block(t_mde_delta_h *dh, t_enxblock *blk)
{
    int i;

    /* first check which type we should use: histogram or raw data */
    if (dh->nhist == 0)
    {
        /* We write raw data.
           Raw data consists of 3 subblocks: a block with the 
           the foreign lambda, and the data itself */
        add_subblocks_enxblock(blk, 3);

        blk->id=enxDH;

        /* subblock 1 */
        dh->subblock_i[0]=dh->derivative ? 1 : 0; /* derivative type */
        blk->sub[0].nr=1;
        blk->sub[0].type=xdr_datatype_int;
        blk->sub[0].ival=dh->subblock_i;

        /* subblock 2 */
        dh->subblock_d[0]=dh->lambda;
        blk->sub[1].nr=1;
        blk->sub[1].type=xdr_datatype_double;
        blk->sub[1].dval=dh->subblock_d;

        /* subblock 3 */
        /* check if there's actual data to be written. */
        //if (dh->ndh > 1)
        if (dh->ndh > 0)
        {
            blk->sub[2].nr=dh->ndh;
/* For F@H for now. */
#undef GMX_DOUBLE
#ifndef GMX_DOUBLE
            blk->sub[2].type=xdr_datatype_float;
            for(i=0;i<dh->ndh;i++)
            {
                dh->dhf[i] = (float)dh->dh[i];
            }
            blk->sub[2].fval=dh->dhf;
#else
            blk->sub[2].type=xdr_datatype_double;
            blk->sub[2].dval=dh->dh;
#endif
            dh->written=TRUE;
        }
        else
        {
            blk->sub[2].nr=0;
#ifndef GMX_DOUBLE
            blk->sub[2].type=xdr_datatype_float;
            blk->sub[2].fval=NULL;
#else
            blk->sub[2].type=xdr_datatype_double;
#endif
            blk->sub[2].dval=NULL;
        }
    }
    else
    {
        int nhist_written=0;
        int i;

        /* check if there's actual data to be written. */
        if (dh->ndh > 1)
        {
            gmx_bool prev_complete=FALSE;
            /* Make the histogram(s) */
            for(i=0;i<dh->nhist;i++)
            {
                if (!prev_complete)
                {
                    /* the first histogram is always normal, and the 
                       second one is always reverse */
                    mde_delta_h_make_hist(dh, i, i==1);
                    nhist_written++;
                    /* check whether this histogram contains all data: if the
                       last bin is 0, it does */
                    if (dh->bin[i][dh->nbins-1] == 0)
                        prev_complete=TRUE;
                    if (!dh->derivative)
                        prev_complete=TRUE;
                }
            }
            dh->written=TRUE;
        }

        /* A histogram consists of 2, 3 or 4 subblocks: 
           the foreign lambda value + histogram spacing, the starting point, 
           and the histogram data (0, 1 or 2 blocks). */
        add_subblocks_enxblock(blk, nhist_written+2);
        blk->id=enxDHHIST;

        /* subblock 1: the foreign lambda value + the histogram spacing */
        dh->subblock_d[0]=dh->lambda;
        dh->subblock_d[1]=dh->dx;
        blk->sub[0].nr=2;
        blk->sub[0].type=xdr_datatype_double;
        blk->sub[0].dval=dh->subblock_d;

        /* subblock 2: the starting point(s) as a long integer */
        dh->subblock_l[0]=nhist_written;
        dh->subblock_l[1]=dh->derivative ? 1 : 0;
        for(i=0;i<nhist_written;i++)
            dh->subblock_l[2+i]=dh->x0[i];

        blk->sub[1].nr=nhist_written+2;
        blk->sub[1].type=xdr_datatype_large_int;
        blk->sub[1].lval=dh->subblock_l;

        /* subblock 3 + 4 : the histogram data */
        for(i=0;i<nhist_written;i++)
        {
            blk->sub[i+2].nr=dh->maxbin[i]+1; /* it's +1 because size=index+1 
                                                 in C */
            blk->sub[i+2].type=xdr_datatype_int;
            blk->sub[i+2].ival=dh->bin[i];
        }
    }
}

/* initialize the collection*/
void mde_delta_h_coll_init(t_mde_delta_h_coll *dhc, const t_inputrec *ir)
{
    int i; 
    double lambda;
    int ndhmax=ir->nstenergy/ir->nstcalcenergy;
    
    dhc->temperature=ir->opts.ref_t[0];  /* only store system temperature */
    dhc->start_time=0.;
    dhc->delta_time=ir->delta_t*ir->nstdhdl;
    dhc->start_time_set=FALSE;

    /* for continuous change of lambda values */
    dhc->start_lambda=ir->fepvals->init_lambda; 
    dhc->delta_lambda=ir->fepvals->delta_lambda*ir->nstdhdl;
    
    /* total number of raw data points in the sample */
    dhc->ndh = 0;
    
    /* include one more for the specification of the state, by lambda or fep_state, store as double for now*/
    if (ir->fepvals->elamstats > elamstatsNO) {
        dhc->ndh +=1;
    }
    
    /* whether to print energies */
    if (ir->fepvals->bPrintEnergy) {
        dhc->ndh += 1;  
    }
    
    /* add the dhdl's */
    for (i=0;i<efptNR;i++)
    {
        if (ir->fepvals->separate_dvdl[i])
        {
            dhc->ndh+=1;
        }
    }
    
    /* add the lambdas */
    dhc->ndh += ir->fepvals->n_lambda;
    
    if (ir->epc > epcNO) {
        dhc->ndh += 1;  /* include pressure-volume work */
    }
    
    snew(dhc->dh, dhc->ndh);
    for(i=0;i<dhc->ndh;i++)
    {
        mde_delta_h_init(dhc->dh+i, ir->fepvals->dh_hist_size, 
                         ir->fepvals->dh_hist_spacing, ndhmax);
    }
}

/* add a bunch of samples - note fep_state is double to allow for better data storage */
void mde_delta_h_coll_add_dh(t_mde_delta_h_coll *dhc, 
                             double fep_state,  
                             double energy,
                             double pV,
                             double *current_lambda,
                             int bExpanded,
                             int bPrintEnergy,
                             int bPressure,
                             int ndhdl,
                             int nlambda,
                             double *dhdl,
                             double *foreign_dU, 
                             double time)
{
    int i,n;
    
    if (!dhc->start_time_set)
    {
        dhc->start_time_set=TRUE;
        dhc->start_time=time;
    }

    n = 0;
    if (bExpanded) 
    {
        mde_delta_h_add_dh(dhc->dh+n,fep_state,time);
        n++;
    }            
    if (bPrintEnergy) 
    {
        mde_delta_h_add_dh(dhc->dh+n,energy,time);
        n++;
    }
    for (i=0;i<ndhdl;i++) 
    {
        mde_delta_h_add_dh(dhc->dh+n, dhdl[i], time);
        n++;
    }
    for (i=0;i<nlambda;i++) 
    {
        mde_delta_h_add_dh(dhc->dh+n, foreign_dU[i], time);
        n++;
    }
    if (bPressure)
    {
        mde_delta_h_add_dh(dhc->dh+n, pV, time);
        n++;
    }
}

/* write the data associated with all the du blocks, but not the blocks 
   themselves. Essentially, the metadata.  Or -- is this generated every time?*/

void mde_delta_h_coll_handle_block(t_mde_delta_h_coll *dhc,
                                   t_enxframe *fr, int nblock)
{
    int i;
    t_enxblock *blk;

    /* add one block with one subblock as the collection's own data */
    nblock++;
    add_blocks_enxframe(fr, nblock);
    blk=fr->block + (nblock-1);

    add_subblocks_enxblock(blk, 1);

    dhc->subblock_d[0] = dhc->temperature; /* temperature */
    dhc->subblock_d[1] = dhc->start_time; /* time of first sample */
    dhc->subblock_d[2] = dhc->delta_time; /* time difference between samples */
    dhc->subblock_d[3] = dhc->start_lambda; /* lambda at starttime */
    dhc->subblock_d[4] = dhc->delta_lambda; /* lambda diff. between samples */
    blk->id=enxDHCOLL;
    blk->sub[0].nr=5;
    blk->sub[0].type=xdr_datatype_double;
    blk->sub[0].dval=dhc->subblock_d;

    for(i=0;i<dhc->ndh;i++)
    {
        nblock++;
        add_blocks_enxframe(fr, nblock);
        blk=fr->block + (nblock-1);

        mde_delta_h_handle_block(dhc->dh+i, blk);
    }
}

/* reset the data for a new round */
void mde_delta_h_coll_reset(t_mde_delta_h_coll *dhc)
{
    int i;
    for(i=0;i<dhc->ndh;i++)
    {
        if (dhc->dh[i].written)
        {
            /* we can now throw away the data */
            mde_delta_h_reset(dhc->dh + i);
        }
    }
    dhc->start_time_set=FALSE;
}

/* set the energyhistory variables to save state */
void mde_delta_h_coll_update_energyhistory(t_mde_delta_h_coll *dhc,
                                           energyhistory_t *enerhist)
{
    int i;
    if (!enerhist->dht)
    {
        snew(enerhist->dht, 1);
        snew(enerhist->dht->ndh, dhc->ndh);
        snew(enerhist->dht->dh, dhc->ndh);
        enerhist->dht->nndh=dhc->ndh;
    }
    else
    {
        if (enerhist->dht->nndh != dhc->ndh)
            gmx_incons("energy history number of delta_h histograms != inputrec's number");
    }
    for(i=0;i<dhc->ndh;i++)
    {
        enerhist->dht->dh[i] = dhc->dh[i].dh;
        enerhist->dht->ndh[i] = dhc->dh[i].ndh;
    }
    enerhist->dht->start_time=dhc->start_time;
    enerhist->dht->start_lambda=dhc->start_lambda;
}



/* restore the variables from an energyhistory */
void mde_delta_h_coll_restore_energyhistory(t_mde_delta_h_coll *dhc,
                                            energyhistory_t *enerhist)
{
    int i;
    unsigned int j;

    if (dhc && !enerhist->dht)
        gmx_incons("No delta_h histograms in energy history");
    if (enerhist->dht->nndh != dhc->ndh)
        gmx_incons("energy history number of delta_h histograms != inputrec's number");

    for(i=0;i<enerhist->dht->nndh;i++)
    {
        dhc->dh[i].ndh=enerhist->dht->ndh[i];
        for(j=0;j<dhc->dh[i].ndh;j++)
        {
            dhc->dh[i].dh[j] = enerhist->dht->dh[i][j];
        }
    }
    dhc->start_time=enerhist->dht->start_time;
    if (enerhist->dht->start_lambda_set)
        dhc->start_lambda=enerhist->dht->start_lambda;
    if (dhc->dh[0].ndh > 0)
        dhc->start_time_set=TRUE;
    else
        dhc->start_time_set=FALSE;
}




