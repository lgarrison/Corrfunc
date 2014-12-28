#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "../utils/utils.h"
#include "../utils/cellarray.h"
#include "../utils/gridlink.h"

#ifdef USE_AVX
#include "avx_calls.h"
#endif

void countpairs(const int ND1,
								const DOUBLE *x1, const DOUBLE *y1, const DOUBLE *z1,
								const int ND2,
								const DOUBLE *x2,  const DOUBLE *y2, const DOUBLE *z2,
								const DOUBLE xmin, const DOUBLE xmax,
								const DOUBLE ymin, const DOUBLE ymax,
								const DOUBLE zmin, const DOUBLE zmax,
								const int autocorr,
								const double rpmax,
#ifdef USE_OMP
								const int nthreads,
#endif
								const int nrpbin,const double * restrict rupp,
								const double pimax, const int npibin)
{

  int bin_refine_factor=1;
	int zbin_refine_factor=2;
	if(autocorr==1) {
		bin_refine_factor=2;
		zbin_refine_factor=2;
	} else {
		bin_refine_factor=1;
		zbin_refine_factor=1;
	}
#ifdef USE_OMP
	if(numthreads > 1) {
		if(autocorr==1) {
			bin_refine_factor=1;
			zbin_refine_factor=1;
		} else {
			bin_refine_factor=1;
			zbin_refine_factor=1;
		}
	}
#endif


	/*---Create 3-D lattice--------------------------------------*/
	int nmesh_x=0,nmesh_y=0,nmesh_z=0;

	cellarray *lattice1 = gridlink(ND1, X1, Y1, Z1, xmin, xmax, ymin, ymax, zmin, zmax, rpmax, bin_refine_factor, bin_refine_factor, zbin_refine_factor, &nmesh_x, &nmesh_y, &nmesh_z);
	cellarray *lattice2 = NULL;
	if(autocorr==0) {
		int ngrid2_x=0,ngrid2_y=0,ngrid2_z=0;
		lattice2 = gridlink(ND2, X2, Y2, Z2, xmin, xmax, ymin, ymax, zmin, zmax, rpmax, bin_refine_factor, bin_refine_factor, zbin_refine_factor, &ngrid2_x, &ngrid2_y, &ngrid2_z);
		assert(nmesh_x == ngrid2_x && "Both lattices have the same number of X bins");
		assert(nmesh_y == ngrid2_y && "Both lattices have the same number of Y bins");
		assert(nmesh_z == ngrid2_z && "Both lattices have the same number of Z bins");
	} else {
		lattice2 = lattice1;
	}

#ifdef PERIODIC
const DOUBLE xdiff = (xmax-xmin);
const DOUBLE ydiff = (ymax-ymin);
const DOUBLE zdiff = (zmax-zmin);
#endif
	

	DOUBLE logrpmax,logrpmin,dlogrp;
  DOUBLE dpi,inv_dpi;
  DOUBLE rupp_sqr[nrpbin];
	const int64_t totnbins = (npibin+1)*(nrpbin+1);

	
#ifndef USE_OMP
	unsigned int npairs[totnbins];
	DOUBLE rpavg[totnbins];
	for(int ibin=0;ibin<totnbins;ibin++) {
		npairs[ibin]=0;
		rpavg[ibin] = 0.0;
	}
#else
	omp_set_num_threads(numthreads);
	unsigned int **all_npairs = (unsigned int **) matrix_calloc(sizeof(unsigned int), numthreads, totnbins);
	DOUBLE ***all_rpavg = (DOUBLE **) matrix_calloc(sizeof(DOUBLE),numthreads,totnbins);
#endif

	DOUBLE rupp_sqr[nrpbin];
	for(int i=0; i < nrpbin;i++) {
		rupp_sqr[i] = rupp[i]*rupp[i];
	}
	
#ifdef USE_AVX
  AVX_FLOATS m_rupp_sqr[nrpbin];
  AVX_FLOATS m_kbin[nrpbin];
  for(int i=0;i<nrpbin;i++) {
    m_rupp_sqr[i] = AVX_SET_FLOAT(rupp_sqr[i]);
    m_kbin[i] = AVX_SET_FLOAT((DOUBLE) i);
  }
#endif
  

  dpi = pimax/(DOUBLE)npibin ;
  inv_dpi = 1.0/dpi;
  
	int64_t totncells = (int64_t) nmesh_x * (int64_t) nmesh_y * (int64_t) nmesh_z;
	
#ifdef USE_OMP
	#pragma omp parallel
	{
		int tid = omp_get_thread_num();
		unsigned int npairs[totnbins];
		DOUBLE rpavg[totnbins];
		for(int i=0;i<totnbins;i++) {
			npairs[i] = 0;
			rpavg[i] = 0.0;
		}

#pragma omp for  schedule(dynamic)
#endif
		/*---Loop-over-lattice1--------------------*/
		for(int icell=0;icell<totncells;icell++) {
			cellarray *first = &(lattice1[icell]);
			int iz = icell % nmesh_z ;
		int ix = icell / (nmesh_z * nmesh_y) ;
		int iy = (icell - iz - ix*nmesh_z*nmesh_y)/nmesh_z ;
		assert( ((iz + nmesh_z*iy + nmesh_z*nmesh_y*ix) == icell) && "Index reconstruction is wrong");
		
	for(int iix=-bin_refine_factor;iix<=bin_refine_factor;iix++){
	  int iiix;
#ifdef PERIODIC
	  DOUBLE off_xwrap=0.0;
	  if(ix + iix >= nmesh_x) {
	    off_xwrap = -xdiff;
	  } else if (ix + iix < 0) {
	    off_xwrap = xdiff;
	  }
	  iiix=(ix+iix+nmesh_x)%nmesh_x;
#else
	  iiix = iix+ix;
	  if(iiix < 0 || iiix >= nmesh_x) {
	    continue;
	  }
#endif

	  for(int iiy=-bin_refine_factor;iiy<=bin_refine_factor;iiy++){
	    int iiiy;
#ifdef PERIODIC
	    DOUBLE off_ywrap = 0.0;
	    if(iy + iiy >= nmesh_y) {
	      off_ywrap = -ydiff;
	    } else if (iy + iiy < 0) {
	      off_ywrap = ydiff;
	    }
	    iiiy=(iy+iiy+nmesh_y)%nmesh_y;
#else
	    iiiy = iiy+iy;
	    if(iiiy < 0 || iiiy >= nmesh_y) {
	      continue;
	    }
#endif
    
	    for(int iiz=-zbin_refine_factor;iiz<=zbin_refine_factor;iiz++){
	      int iiiz;
#ifdef PERIODIC
	      DOUBLE off_zwrap = 0.0;
	      if(iz + iiz >= nmesh_z) {
		off_zwrap = -zdiff;
	      } else if (iz + iiz < 0) {
		off_zwrap = zdiff;
	      }
	      iiiz=(iz+iiz+nmesh_z)%nmesh_z;
#else
	      iiiz = iiz+iz;
	      if(iiiz < 0 || iiiz >= nmesh_z) {
		continue;
	      }
#endif
	      
	      assert(iiix >= 0 && iiix < nmesh_x && iiiy >= 0 && iiiy < nmesh_y && iiiz >= 0 && iiiz < nmesh_z && "Checking that the second pointer is in range");
	      cellarray *second = &(lattice2[iiix][iiiy][iiiz]);
	      DOUBLE *x1 = first->x;
	      DOUBLE *y1 = first->y;
	      DOUBLE *z1 = first->z;

	      DOUBLE *x2 = second->x;
	      DOUBLE *y2 = second->y;
	      DOUBLE *z2 = second->z;

	      for(int i=0;i<first->nelements;i++){
		DOUBLE x1pos = x1[i];
		DOUBLE y1pos = y1[i];
		DOUBLE z1pos = z1[i];
#ifdef PERIODIC
		x1pos += off_xwrap;
		y1pos += off_ywrap;
		z1pos += off_zwrap;
#endif

#ifndef USE_AVX	//Beginning of NO AVX section
		for(int j=0;j<second->nelements;j++) {
		  DOUBLE dx = x2[j]-x1pos;
		  DOUBLE dy = y2[j]-y1pos;
		  DOUBLE dz = z2[j]-z1pos;
		  
		  dz = FABS(dz);
		  DOUBLE r2 = dx*dx + dy*dy;
		  if(r2 >= sqr_rpmax || r2 < sqr_rpmin || dz >= pimax) {
		    continue;
		  }
		  
		  DOUBLE r = SQRT(r2);
		  int pibin = (int) (dz*inv_dpi);
		  pibin = pibin > npibin ? npibin:pibin;
		  for(int kbin=nrpbin-1;kbin>=0;kbin--) {
		    if(r2 >= rupp_sqr[kbin]) {
		      int ibin = kbin*(npibin+1) + pibin;
		      npairs[ibin]++;
		      rpavg[ibin]+=r;
		      break;
		    }
		  }
		}

#else //beginning of AVX section
                union int8 {
		  AVX_INTS m_ibin;
		  int ibin[NVEC];
		};
		union int8 union_rpbin;
		union int8 union_pibin;

		union float8{
		  AVX_FLOATS m_Dperp;
		  DOUBLE Dperp[NVEC];
		};
		union float8 union_mDperp;
		
		AVX_FLOATS m_x1pos = AVX_SET_FLOAT(x1[i]);
		AVX_FLOATS m_y1pos = AVX_SET_FLOAT(y1[i]);
		AVX_FLOATS m_z1pos = AVX_SET_FLOAT(z1[i]);
#ifdef PERIODIC
		{
		  //new scope
		  AVX_FLOATS m_off_xwrap = AVX_SET_FLOAT(off_xwrap);
		  AVX_FLOATS m_off_ywrap = AVX_SET_FLOAT(off_ywrap);
		  AVX_FLOATS m_off_zwrap = AVX_SET_FLOAT(off_zwrap);
		  
		  m_x1pos = AVX_ADD_FLOATS(m_x1pos, m_off_xwrap);
		  m_y1pos = AVX_ADD_FLOATS(m_y1pos, m_off_ywrap);
		  m_z1pos = AVX_ADD_FLOATS(m_z1pos, m_off_zwrap);
		}
#endif

		int j;
		for(j=0;j<=(second->nelements-NVEC);j+=NVEC) {
                  AVX_FLOATS x2pos = AVX_LOAD_FLOATS_UNALIGNED(&x2[j]);
		  AVX_FLOATS y2pos = AVX_LOAD_FLOATS_UNALIGNED(&y2[j]);
		  AVX_FLOATS z2pos = AVX_LOAD_FLOATS_UNALIGNED(&z2[j]);
		  AVX_FLOATS m_sqr_rpmax = AVX_SET_FLOAT(sqr_rpmax);
		  AVX_FLOATS m_sqr_rpmin = AVX_SET_FLOAT(sqr_rpmin);
		  /* AVX_FLOATS m_rpmax = AVX_SET_FLOAT(rpmax); */
                  AVX_FLOATS m_pimax = AVX_SET_FLOAT(pimax);
		  AVX_FLOATS m_zero  = AVX_SET_FLOAT((DOUBLE) 0.0);
		  AVX_FLOATS m_inv_dpi    = AVX_SET_FLOAT(inv_dpi);
		  
		  AVX_FLOATS m_xdiff = AVX_SUBTRACT_FLOATS(m_x1pos,x2pos);
		  AVX_FLOATS m_ydiff = AVX_SUBTRACT_FLOATS(m_y1pos,y2pos);
		  AVX_FLOATS m_zdiff = AVX_SUBTRACT_FLOATS(m_z1pos,z2pos);
		  m_zdiff = AVX_MAX_FLOATS(m_zdiff,AVX_SUBTRACT_FLOATS(m_zero,m_zdiff));//dz = fabs(dz) => dz = max(dz, -dz);
		  
		  AVX_FLOATS m_dist  = AVX_ADD_FLOATS(AVX_SQUARE_FLOAT(m_xdiff),AVX_SQUARE_FLOAT(m_ydiff));
		  AVX_FLOATS m_mask_left;

		  //Do all the distance cuts using masks here in new scope
		  {
		    AVX_FLOATS m_mask_pimax = AVX_COMPARE_FLOATS(m_zdiff,m_pimax,_CMP_LT_OS);
		    int test = AVX_TEST_COMPARISON(m_mask_pimax);
		    if(test == 0) {
		      continue;
		    }
		    AVX_FLOATS m1 = AVX_COMPARE_FLOATS(m_dist,m_sqr_rpmin,_CMP_GE_OS);
		    m_dist = AVX_BLEND_FLOATS_WITH_MASK(m_sqr_rpmax,m_dist,m_mask_pimax);

		    m_mask_left = AVX_COMPARE_FLOATS(m_dist,m_sqr_rpmax,_CMP_LT_OS);//will get utilized in the next section
		    AVX_FLOATS m_mask = AVX_BITWISE_AND(m1,m_mask_left);
		    int test1 = AVX_TEST_COMPARISON(m_mask);
		    if(test1 == 0) {
		      continue;
		    }

		    //So there's at least one point that is in range - let's find the bin
		    union_mDperp.m_Dperp = AVX_SQRT_FLOAT(m_dist);
		    m_zdiff = AVX_BLEND_FLOATS_WITH_MASK(m_pimax, m_zdiff, m_mask);
		    union_pibin.m_ibin = AVX_TRUNCATE_FLOAT_TO_INT(AVX_MULTIPLY_FLOATS(m_zdiff,m_inv_dpi));
		  }

		  {
		    AVX_FLOATS m_rpbin     = AVX_SET_FLOAT((DOUBLE) nrpbin);
		    AVX_FLOATS m_all_ones  = AVX_CAST_INT_TO_FLOAT(AVX_SET_INT(-1));
		    for(int kbin=nrpbin-1;kbin>=0;kbin--) {
		      AVX_FLOATS m_mask_low = AVX_COMPARE_FLOATS(m_dist,m_rupp_sqr[kbin],_CMP_GE_OS);
		      AVX_FLOATS m_bin_mask = AVX_BITWISE_AND(m_mask_low,m_mask_left);
		      m_rpbin = AVX_BLEND_FLOATS_WITH_MASK(m_rpbin,m_kbin[kbin], m_bin_mask);
		      //m_mask_left = AVX_COMPARE_FLOATS(m_dist, m_rupp_sqr[kbin],_CMP_LT_OS);
		      m_mask_left = AVX_XOR_FLOATS(m_mask_low, m_all_ones);//XOR with 0xFFFF... gives the bins that are smaller than m_rupp_sqr[kbin] (and is faster than cmp_p(s/d) in theory)
		      int test = AVX_TEST_COMPARISON(m_mask_left);
		      if(test==0)
			break;
		    }
		    union_rpbin.m_ibin = AVX_TRUNCATE_FLOAT_TO_INT(m_rpbin);
		  }
		  
		  //update the histograms
#if defined(__ICC) || defined(__INTEL_COMPILER)
#pragma unroll(NVEC)
#endif		  
		  for(int jj=0;jj<NVEC;jj++) {
		    int rpbin = union_rpbin.ibin[jj];
		    int pibin = union_pibin.ibin[jj];
		    int ibin = rpbin*(npibin+1) + pibin;
		    npairs[ibin]++;
		    rpavg [ibin] += union_mDperp.Dperp[jj];
		  }
		}

		//remainder loop
		for(;j<second->nelements;j++) {
		  DOUBLE dx = x2[j]-x1pos;
		  DOUBLE dy = y2[j]-y1pos;
		  DOUBLE dz = z2[j]-z1pos;
		  
		  dz = FABS(dz);
		  DOUBLE r2 = dx*dx + dy*dy;
		  if(r2 >= sqr_rpmax || r2 < sqr_rpmin || dz >= pimax) {
		    continue;
		  }

		  DOUBLE r = SQRT(r2);
		  int pibin = (int) (dz*inv_dpi);
		  pibin = pibin > npibin ? npibin:pibin;
		  for(int kbin=nrpbin-1;kbin>=0;kbin--) {
		    if(r2 >= rupp_sqr[kbin]) {
		      int ibin = kbin*(npibin+1) + pibin;
		      npairs[ibin]++;
		      rpavg [ibin] += r;
		      break;
		    }
		  }
		} //end of j-remainder loop

#endif //end of AVX section

	      }//end of i-loop over first
	    }//iiz loop over zbin_refine_factor
	  }//iiy loop over bin_refine_factor
	}//iix loop over bin_refine_factor
		}//icell loop over totncells
#ifdef USE_OMP
		for(int j=0;j<nrpbin;j++) {
			all_npairs[tid][j] = npairs[j];
			all_rpavg[tid][j] = rpavg[j];
		}
	}//close the omp parallel region
#endif
	
#ifdef USE_OMP
	unsigned int npairs[totnbins]
	DOUBLE rpavg[totnbins];
	for(int i=0;i<totnbins;i++) {
		npairs[i] = 0;
		rpavg[i] = 0.0;
	}

	for(int i=0;i<numthreads;i++) {
		for(int j=0;j<totnbins;j++) {
			npairs[j] += all_npairs[i][j];
			rpavg[j] += all_rpavg[i][j];
		}
	}
#endif
	
	for(int i=0;i<=nrpbin;i++) {
		for(int j=0;j<=npibin;j++) {
			index = i*(npibin+1) + j;
			if(npairs[index] > 0) {
				rpavg[index] /= ((DOUBLE) npairs[index] );
			}
		}
	}


  //rp's are all in log2 -> convert to log10
  const double inv_log10=1.0/log2(10);
  for(int i=0;i<nrpbin;i++) {
    DOUBLE logrp = logrpmin + (DOUBLE)(i+1)*dlogrp;
    for(int j=0;j<npibin;j++) {
      index = i*(npibin+1) + j;
      fprintf(stdout,"%10"PRIu64" %20.8lf %20.8lf  %20.8lf \n",npairs[index],rpavg[index],logrp*inv_log10,(j+1)*dpi);
    }
  }

  for(int64_t i=0;i<totncells;i++) {
		free(lattice1[i].x);
		free(lattice1[i].y);
		free(lattice1[i].z);
		if(autocorr==0) {
			free(lattice2[i].x);
			free(lattice2[i].y);
			free(lattice2[i].z);
		}
	}

	free(lattice1);
	if(autocorr==0) {
		free(lattice2);
	}
#ifdef USE_OMP
	matrix_free((void **) all_npairs, numthreads);
	matrix_free((void **) all_rpavg, numthreads);
#endif
	

}
