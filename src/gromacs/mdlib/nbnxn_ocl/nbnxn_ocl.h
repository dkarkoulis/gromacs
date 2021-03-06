/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2012,2013,2014, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */

#ifndef NBNXN_OCL_H
#define NBNXN_OCL_H

#include "config.h"

#include "types/nbnxn_ocl_types_ext.h"
#include "types/simple.h"

#if defined(GMX_GPU) && defined(GMX_USE_OPENCL)
#define FUNC_TERM ;
#else
#define FUNC_TERM {}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief
 * Launch asynchronously the nonbonded force calculations.
 *
 *  This consists of the following (async) steps launched:
 *  - upload x and q;
 *  - upload shift vector;
 *  - launch kernel;
 *  The local and non-local interaction calculations are launched in two
 *  separate streams.
 */
void nbnxn_ocl_launch_kernel(nbnxn_opencl_ptr_t       gmx_unused  cu_nb,
                              const nbnxn_atomdata_t gmx_unused *nbdata,
                              int                    gmx_unused  flags,
                              int                    gmx_unused  iloc) FUNC_TERM

/*! \brief
 * Launch asynchronously the download of nonbonded forces from the GPU
 * (and energies/shift forces if required).
 */
void nbnxn_ocl_launch_cpyback(nbnxn_opencl_ptr_t       gmx_unused  cu_nb,
                               const nbnxn_atomdata_t gmx_unused *nbatom,
                               int                    gmx_unused  flags,
                               int                    gmx_unused  aloc) FUNC_TERM

/*! \brief
 * Wait for the asynchronously launched nonbonded calculations and data
 * transfers to finish.
 */
void nbnxn_ocl_wait_gpu(nbnxn_opencl_ptr_t      gmx_unused  cu_nb,
                         const nbnxn_atomdata_t gmx_unused *nbatom,
                         int                    gmx_unused  flags,
                         int                    gmx_unused  aloc,
                         real                   gmx_unused *e_lj,
                         real                   gmx_unused *e_el,
                         rvec                   gmx_unused *fshift) FUNC_TERM

#ifdef __cplusplus
}
#endif

#undef FUNC_TERM

#endif /* NBNXN_CUDA_H */
