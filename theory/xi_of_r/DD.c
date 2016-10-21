/* File: DD.c */
/*
  This file is a part of the Corrfunc package
  Copyright (C) 2015-- Manodeep Sinha (manodeep@gmail.com)
  License: MIT LICENSE. See LICENSE file under the top-level
  directory at https://github.com/manodeep/Corrfunc/
*/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <inttypes.h>

#include "defs.h" //for ADD_DIFF_TIME
#include "function_precision.h" //definition of DOUBLE
#include "countpairs.h" //function proto-type for countpairs
#include "io.h" //function proto-type for file input
#include "utils.h" //general utilities

void Printhelp(void);

int main(int argc, char *argv[])
{

    /*---Data-variables--------------------*/
    int64_t ND1,ND2 ;

    DOUBLE *x1=NULL,*y1=NULL,*z1=NULL,*weights1[MAX_NUM_WEIGHTS]={NULL};
    DOUBLE *x2=NULL,*y2=NULL,*z2=NULL,*weights2[MAX_NUM_WEIGHTS]={NULL};

    char *file1=NULL,*file2=NULL,*weights_file1=NULL,*weights_file2=NULL;
    char *fileformat1=NULL,*fileformat2=NULL,*weights_fileformat1=NULL,*weights_fileformat2=NULL;
    char *binfile=NULL;
    char *weight_method_str=NULL;
    
    weight_method_t weight_method = NONE;
    int num_weights = 0;

    /*---Corrfunc-variables----------------*/
#if !(defined(USE_OMP) && defined(_OPENMP))
    const int nthreads=1;
    const char argnames[][30]={"file1","format1","file2","format2","binfile"};
#else
    int nthreads=2;
    const char argnames[][30]={"file1","format1","file2","format2","binfile","Nthreads"};
#endif
    const char optargnames[][30]={"weight_method", "weights_file1","weights_format1","weights_file2","weights_format2"};
    
    int nargs=sizeof(argnames)/(sizeof(char)*30);
    int noptargs=sizeof(optargnames)/(sizeof(char)*30);

    struct timeval t_end,t_start,t0,t1;
    double read_time=0.0;

    gettimeofday(&t_start,NULL);

    /*---Read-arguments-----------------------------------*/
    if(argc< (nargs+1)) {
        Printhelp() ;
        fprintf(stderr,"\nFound: %d parameters\n ",argc-1);
        int i;
        for(i=1;i<argc;i++) {
            if(i <= nargs)
                fprintf(stderr,"\t\t %s = `%s' \n",argnames[i-1],argv[i]);
            else if(i <= nargs + noptargs)
                fprintf(stderr,"\t\t %s = `%s' \n",optargnames[i-1-nargs],argv[i]);
            else
                fprintf(stderr,"\t\t <> = `%s' \n",argv[i]);
        }
        fprintf(stderr,"\nMissing required parameters \n");
        for(i=argc;i<=nargs;i++)
            fprintf(stderr,"\t\t %s = `?'\n",argnames[i-1]);
        return EXIT_FAILURE;
    }
  
    /* Validate optional arguments */
    int noptargs_given = argc - (nargs + 1);
    if(noptargs_given != 0 && noptargs_given != 3 && noptargs_given != 5){
        Printhelp();
        fprintf(stderr,"\nFound: %d optional arguments; must be 0 (no weights), 3 (for one set of weights) or 5 (for two sets)\n ", noptargs_given);
        int i;
        for(i=nargs+1;i<argc;i++) {
            if(i <= nargs + noptargs)
                fprintf(stderr,"\t\t %s = `%s' \n",optargnames[i-nargs-1],argv[i]);
            else
                fprintf(stderr,"\t\t <> = `%s' \n",argv[i]);
        }
        return EXIT_FAILURE;

    }

    file1=argv[1];
    fileformat1=argv[2];
    file2=argv[3];
    fileformat2=argv[4];
    binfile=argv[5];

#if defined(USE_OMP) && defined(_OPENMP)
    nthreads=atoi(argv[6]);
    if(nthreads < 1) {
        fprintf(stderr,"Error: Nthreads must be at least 1...returning\n");
        return EXIT_FAILURE;
    }
#endif
  
    if(noptargs_given >= 3){
       weight_method_str = argv[nargs + 1];
       int wstatus = get_weight_method_by_name(weight_method_str, &weight_method);
       if(wstatus != EXIT_SUCCESS){
         fprintf(stderr, "Error: Unknown weight method \"%s\"\n", weight_method_str);
         return EXIT_FAILURE;
       }
       num_weights = get_num_weights_by_method(weight_method);
      
       weights_file1 = argv[nargs + 2];
       weights_fileformat1 = argv[nargs + 3];
    }
    if(noptargs_given >= 5){
       weights_file2 = argv[nargs + 4];
       weights_fileformat2 = argv[nargs + 5];
    }

    fprintf(stderr,"Running `%s' with the parameters \n",argv[0]);
    fprintf(stderr,"\n\t\t -------------------------------------\n");
    for(int i=1;i<argc;i++) {
        if(i <= nargs) {
            fprintf(stderr,"\t\t %-10s = %s \n",argnames[i-1],argv[i]);
        } else if(i <= nargs + noptargs){
            fprintf(stderr,"\t\t %-10s = %s \n",optargnames[i-nargs-1],argv[i]);
        } else {
            fprintf(stderr,"\t\t <> = `%s' \n",argv[i]);
        }
    }
    fprintf(stderr,"\t\t -------------------------------------\n");



    /*---Read-data1-file----------------------------------*/
    gettimeofday(&t0,NULL);
    ND1=read_positions(file1,fileformat1, sizeof(DOUBLE), 3, &x1, &y1, &z1);
    gettimeofday(&t1,NULL);
    read_time += ADD_DIFF_TIME(t0,t1);

    int autocorr=0;
    if( strcmp(file1,file2)==0) {
        autocorr=1;
    }
  
    /* Read weights file 1 */
    if(weights_file1 != NULL){
        gettimeofday(&t0,NULL);
        int64_t wND1 = read_columns_into_array(weights_file1,weights_fileformat1, sizeof(DOUBLE), num_weights, (void **) weights1);
        gettimeofday(&t1,NULL);
        read_time += ADD_DIFF_TIME(t0,t1);
      
        if(wND1 != ND1){
          fprintf(stderr, "Error: read %"PRId64" lines from %s, but read %"PRId64" from %s\n", wND1, weights_file1, ND1, file1);
          return EXIT_FAILURE;
        }
    }

    gettimeofday(&t0,NULL);
    if (autocorr==0) {
        /*---Read-data2-file----------------------------------*/
        ND2=read_positions(file2,fileformat2, sizeof(DOUBLE), 3, &x2, &y2, &z2);
        gettimeofday(&t1,NULL);
        read_time += ADD_DIFF_TIME(t0,t1);
      
        /* Read weights file 2 */
        if(weights_file2 != NULL){
            gettimeofday(&t0,NULL);
            int64_t wND2 = read_columns_into_array(weights_file2,weights_fileformat2, sizeof(DOUBLE), num_weights, (void **) weights2);
            gettimeofday(&t1,NULL);
            read_time += ADD_DIFF_TIME(t0,t1);

            if(wND2 != ND2){
              fprintf(stderr, "Error: read %"PRId64" lines from %s, but read %"PRId64" from %s\n", wND2, weights_file2, ND2, file2);
              return EXIT_FAILURE;
            }
        }
    } else {
        ND2 = ND1;
        x2 = x1;
        y2 = y1;
        z2 = z1;
        for(int w = 0; w < MAX_NUM_WEIGHTS; w++){
          weights2[w] = weights1[w];
        }
    }

    /*---Count-pairs--------------------------------------*/
    gettimeofday(&t0,NULL);
    struct config_options options = get_config_options();

    /* Pack weights into extra options */
    struct extra_options extra;
    int estatus = get_extra_options(&extra, weight_method);
    if(estatus != EXIT_SUCCESS){
      fprintf(stderr, "Failed to create extra options!  Malloc failure?\n");
    }
    for(int w = 0; w < num_weights; w++){
        extra.weights0.weights[w] = (void *) weights1[w];
        extra.weights1.weights[w] = (void *) weights2[w];
    }

    /* If you want to change the bin refine factors */
    /* const int bf[] = {2, 2, 1}; */
    /* set_bin_refine_factors(&options, bf); */
    results_countpairs results;
    int status = countpairs(ND1,x1,y1,z1,
                            ND2,x2,y2,z2,
                            nthreads,
                            autocorr,
                            binfile,
                            &results,
                            &options,
                            &extra);/* This is for ABI compatibility */

    
    free(x1);free(y1);free(z1);
    for(int w = 0; w < num_weights; w++){
        free(weights1[w]);
    }
    if(autocorr == 0) {
        free(x2);free(y2);free(z2);
        for(int w = 0; w < num_weights; w++){
          free(weights2[w]);
        }
    }
    if(status != EXIT_SUCCESS) {
      return status;
    }
    
    gettimeofday(&t1,NULL);
    double pair_time = ADD_DIFF_TIME(t0,t1);

    DOUBLE rlow=results.rupp[0];
    for(int i=1;i<results.nbin;i++) {
        fprintf(stdout,"%e\t%e\t%e\t%12"PRIu64"\t%e\n",rlow, results.rupp[i], results.rpavg[i],results.npairs[i],results.weightavg[i]);
        rlow=results.rupp[i];
    }

    //free the memory in the results struct
    free_results(&results);
    gettimeofday(&t_end,NULL);
    fprintf(stderr,"xi_of_r> Done -  ND1=%"PRId64" ND2=%"PRId64". Time taken = %6.2lf seconds. read-in time = %6.2lf seconds sec pair-counting time = %6.2lf sec\n",
            ND1,ND2,ADD_DIFF_TIME(t_start,t_end),read_time,pair_time);
    return EXIT_SUCCESS;
}

/*---Print-help-information---------------------------*/
void Printhelp(void)
{
    fprintf(stderr,"=========================================================================\n") ;
#if defined(USE_OMP) && defined(_OPENMP)
    fprintf(stderr,"   --- DD file1 format1 file2 format2 binfile numthreads [weight_method weights_file1 weights_format1 [weights_file2 weights_format2]] > DDfile\n");
#else
    fprintf(stderr,"   --- DD file1 format1 file2 format2 binfile [weight_method weights_file1 weights_format1 [weights_file2 weights_format2]] > DDfile\n") ;
#endif
    fprintf(stderr,"   --- Measure the cross-correlation function DD(r) for two different\n") ;
    fprintf(stderr,"       data files (or autocorrelation if file1=file2).\n") ;
    fprintf(stderr,"     * file1         = name of first data file\n") ;
    fprintf(stderr,"     * format1       = format of first data file  (a=ascii, c=csv, f=fast-food)\n") ;
    fprintf(stderr,"     * file2         = name of second data file\n") ;
    fprintf(stderr,"     * format2       = format of second data file (a=ascii, c=csv, f=fast-food)\n") ;
    fprintf(stderr,"     * binfile       = name of ascii file containing the r-bins (rmin rmax for each bin)\n") ;
#if defined(USE_OMP) && defined(_OPENMP)
    fprintf(stderr,"     * numthreads    = number of threads to use\n");
#endif
    fprintf(stderr,"   --- OPTIONAL ARGS:\n");
    fprintf(stderr,"     * weight_method = the type of pair weighting to apply.  Options are: 'pair_product', 'none'.  Default: 'none'.\n");
    fprintf(stderr,"     * weights_file1 = name of file containing the weights corresponding to the first data file\n");
    fprintf(stderr,"     * weights_format1 = name of file containing the weights corresponding to the first data file\n");
    fprintf(stderr,"     * weights_file2 = name of file containing the weights corresponding to the second data file\n");
    fprintf(stderr,"     * weights_format2 = name of file containing the weights corresponding to the second data file\n");
    fprintf(stderr,"   ---OUTPUT:\n") ;
#ifdef OUTPUT_RPAVG
    fprintf(stderr,"     > DDfile        = name of output file <npairs rpavg rmin rmax weightavg>\n") ;
#else
    fprintf(stderr,"     > DDfile        = name of output file <npairs  0.0  rmin rmax weightavg>\n") ;
#endif
    fprintf(stderr,"\n\tCompile options: \n");
#ifdef PERIODIC
    fprintf(stderr,"\tPeriodic = True\n");
#else
    fprintf(stderr,"\tPeriodic = False\n");
#endif

#ifdef OUTPUT_RPAVG
    fprintf(stderr,"\tOutput RPAVG = True\n");
#else
    fprintf(stderr,"\tOutput RPAVG = False\n");
#endif

#ifdef DOUBLE_PREC
    fprintf(stderr,"\tPrecision = double\n");
#else
    fprintf(stderr,"\tPrecision = float\n");
#endif

#if defined(__AVX__)
    fprintf(stderr,"\tUse AVX = True\n");
#else
    fprintf(stderr,"\tUse AVX = False\n");
#endif

#if defined(USE_OMP) && defined(_OPENMP)
    fprintf(stderr,"\tUse OMP = True\n");
#else
    fprintf(stderr,"\tUse OMP = False\n");
#endif

    fprintf(stderr,"=========================================================================\n") ;

}
