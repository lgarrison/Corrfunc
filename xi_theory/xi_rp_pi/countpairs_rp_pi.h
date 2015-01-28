/* File: countpairs_rp_pi.h */
/*
		This file is a part of the Corrfunc package
		Copyright (C) 2015-- Manodeep Sinha (manodeep@gmail.com)
		License: MIT LICENSE. See LICENSE file under the top-level
		directory at https://bitbucket.org/manodeep/corrfunc/
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "function_precision.h" //for definition of DOUBLE
#include <inttypes.h> //for uint64_t

//define the results structure
typedef struct{
	uint64_t *npairs;
	DOUBLE *rupp;
	DOUBLE *rpavg;
	DOUBLE pimax;
	int nbin;
	int npibin;
} results_countpairs_rp_pi;

results_countpairs_rp_pi * countpairs_rp_pi(const int64_t ND1, const DOUBLE *X1, const DOUBLE *Y1, const DOUBLE *Z1,
											const int64_t ND2, const DOUBLE *X2, const DOUBLE *Y2, const DOUBLE *Z2,
#ifdef USE_OMP
											const int numthreads,
#endif
											const int autocorr,
											const char *binfile,
											const double pimax)  __attribute__((warn_unused_result));

void free_results_rp_pi(results_countpairs_rp_pi **results);

#ifdef __cplusplus
}
#endif
