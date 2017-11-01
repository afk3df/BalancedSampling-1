/* Copyright (c) 2015-2017 Jonathan Lisic 
 * Last edit: 17/10/19 - 10:57:03 
 * License: GPL (>=2) 
 */  

#include "lpm3.h"




/* return new prob */
void updateProb( 
    double * xPtr, 
    double * yPtr, 
    double U ) { 

  double xySum = *xPtr + *yPtr;

  if( xySum < 1 ) {

    if( *xPtr / xySum > U ) {
      *xPtr = xySum;
      *yPtr = 0;
      return;
    }

    *xPtr = 0;
    *yPtr = xySum;
    return;
  } 
    
  if( (1 - *yPtr) / (2 - xySum) > U ) {
    *xPtr = 1;
    *yPtr = xySum - 1;
    return;
  } 

  *xPtr = xySum - 1;
  *yPtr = 1;
  return;
}




/* function to update the mapping and inverse mapping */
void updateMapping(size_t j,size_t i, size_t * indexMap, size_t * reverseIndexMap) {

  size_t move, temp;

  move = reverseIndexMap[j];    /* we get where j is now */
  reverseIndexMap[j] = i;       /* we update where it is now to where it will be moved to, i */ 
   
  temp = indexMap[i];           /* we now need to figure out what is hanging out at i */
  reverseIndexMap[temp] = move; /* we now update where it is going to move to (the old location of j) */    
      
  indexMap[i] = j;              /* now we update the mapping */
  indexMap[move] = temp;

  return;
}




/* the R interface */
void R_lpm3(
    double * x,
    double * pi,
    int * nPtr,
    int * KPtr, 
    int * mPtr,
    int * algorithmPtr,
    int * maxCountPtr,
    double * termDist,
    int * recordOrder, 
    int * drawsPtr, /* resampling count */
    int * useProbPtr
  ) {

  size_t n = (size_t) * nPtr;
  size_t m = (size_t) * mPtr;
  size_t K = (size_t) * KPtr;
  size_t useProb = (size_t) * useProbPtr;
  size_t draws = (size_t) * drawsPtr;
  size_t maxCount = (size_t) * maxCountPtr;
  size_t i,j,k,l;
  double tieBreak; /* used to break ties */

  size_t t;

  GetRNGstate();

  size_t order=1;  // record in order
  size_t sampled;
  double dist;
  double * r1;
  double * r2;

  size_t count;

  size_t * backupTreeIndex = NULL;


  /***************************** CREATE RANDOM ***************************/
  r1 = (double *) calloc(n, sizeof(double) );
  r2 = (double *) calloc(n, sizeof(double) );
  /***************************** CREATE RANDOM ***************************/


  /***************************** CREATE INDEX ****************************/
  // note: treeIndex will be destroyed in creating the tree
  size_t * indexMap = (size_t *) calloc( n , sizeof( size_t ) );
  size_t * treeIndex = (size_t *) calloc( n , sizeof( size_t ) ); 
  size_t * reverseIndexMap = (size_t *) calloc( n , sizeof( size_t ) );
  for( i=0; i< n; i++) {
    indexMap[i]=i;
    treeIndex[i]=i;
    reverseIndexMap[i]=i;
  }
  /***************************** CREATE INDEX ****************************/


  /***************************** CREATE TREE *****************************/
  /* 
   K - dim of each element 
   m - leafSize 
   n - number of elements
   x - data  
  */
  rootNodePtr myTree = createTree( K, m, n, x);
      
//  printf("1. building index...");
  if( useProb) { 
    myTree->root = buildIndex( myTree, 0, n, treeIndex, useProb, pi ); 
  } else {
    myTree->root = buildIndex( myTree, 0, n, treeIndex, useProb, NULL ); 
  }
//  printf(" done.\n");
//      printf("\nTree Index\n");
      for( t = 0; t <n; t++) printf("%d ", (int) *(myTree->pointerIndex[t]));  
  /***************************** CREATE TREE *****************************/

  /* save a copy of the tree index */
  if( draws > 1 ) {
    backupTreeIndex = (size_t *) calloc( n , sizeof( size_t ) ); 
    for( i=0; i< n; i++) {
      backupTreeIndex[i] = *(myTree->pointerIndex[i]);  
    }
  }

 
  /***************************** RESAMPLE *****************************/
  for( l=0; l < draws; l++) {

    /* refresh the index */ 
    if( l > 0 ) { 
      /* offset pi */
      /* need to make this more memory efficient */
      pi = pi + n;

      if(recordOrder[0] != -2) {
        recordOrder = recordOrder + n;
      }

      for( i=0; i< n; i++) {
        indexMap[i]=i;
        reverseIndexMap[i]=i;
        *(myTree->pointerIndex[i]) =  backupTreeIndex[i]; 
      }
    } 

    /* generate values ahead of time to be like lpm2 */ 
    for( i = 0; i < n; i++) {
      r1[i] = runif(0.0,1.0);
    }
  
    for( i = 0; i < n; i++) {
      r2[i] = runif(0.0,1.0);
    }
  
  
    /***************************** RUN ALGORITHM ***************************/
    for( i = 0; i < n-1; i++) {
  
      sampled = i + (size_t) floor( r1[i] * (n - i) );
  
      /* randomly select j */
      j = indexMap[sampled]; // j is the original index of the sampled data
  
      if( j >=n  ) break;
  
      // find neighbor
      dist = INFINITY;
      k = n;
      tieBreak = -1;
  
  
//      printf("2. searching...");
      /* algorithm 1 is a count bounded kdtree */
      if( *algorithmPtr == 1 ) {
        count = 0;
        k = find_nn_notMe_count( 
            myTree , 
            myTree->root, 
            j, 
            &(x[j*K]), 
            &dist, 
            &tieBreak,
            &count,
            &maxCount
            ); 
      } else if( *algorithmPtr == 2 ) {
      /* algorithm 2 is a distance bounded kdtree */
        k = find_nn_notMe_dist( 
            myTree , 
            myTree->root, 
            j, 
            &(x[j*K]), 
            &dist, 
            &tieBreak,
            termDist
            ); 
      } else {
      /* algorithm 0 is a standard kd-tree */
        k = find_nn_notMe( 
            myTree , 
            myTree->root, 
            j, 
            &(x[j*K]), 
            &dist, 
            &tieBreak
            ); 
      }
//      printf(" done\n"); 
  
  
    
      /* break if an invalid neighbor is selected */
      if( k >=n  ) {
        PRINTF("breaking on iteration %d, for k\n", (int) i);
        break;
      }
 
//      printf("update prob..."); 
  
      updateProb( 
         &( pi[j] ), 
         &( pi[k] ), 
         r2[i]
         ); 
      
/*      printf(" done\n"); 
  
      printf("i = %d, j = %d", (int) i, (int) j);
      printf("\nTree Index\n");
      for( t = 0; t <n; t++) printf("%d ", (int) *(myTree->pointerIndex[t]));  
      printf("\nindexMap\n");
      for( t = 0; t <n; t++) printf("%d ", (int) indexMap[t]);
      printf("reverseIndexMap\n");
      for( t = 0; t <n; t++) printf("%d ",(int) reverseIndexMap[t]);
      printf("\n");
      
      printf("reverse mapping..."); 
*/
      /* handle reverse mapping etc... */
      /* move is from the reverse mapping since we don't really know the index of k */
      /* it also is a bit more readable for j instead of grabbing sampled again */
      if( pi[j] == 0 || pi[j] == 1 ) {
        updateMapping(j,i,indexMap,reverseIndexMap);
        *(myTree->pointerIndex[j]) = n;  // ensure we can't find it again 
      } else {
        updateMapping(k,i,indexMap,reverseIndexMap);
        *(myTree->pointerIndex[k]) = n;  // ensure we can't find it again 
      }
//      printf(" done\n"); 
        
//      printf("record order..."); 
      // record in order
      if(recordOrder[0] != -2) {
        if( recordOrder[j] == -1 ) {
          if(pi[j] == 1)  {
            recordOrder[j] = (int) order;
            order++;  
          }
        } 
        if(pi[k] == 1) {
          if( recordOrder[k] == -1 ) {
            recordOrder[k] = (int) order;
            order++;  
          }
        } 
      }
//      printf(" done\n"); 
  
  
    } 
    /***************************** RUN ALGORITHM ***************************/

  }
  /***************************** RUN RESAMPLE ***************************/
 
// printf("clean up..."); 

  /* delete tree */
  deleteTree(myTree);

  // free indexes */
  free(indexMap);
  free(reverseIndexMap);
  free(r1);
  free(r2);

  if(backupTreeIndex != NULL) free(backupTreeIndex);  
  
  PutRNGstate();

//  printf("done.\n");
  return;
}





