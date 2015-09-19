/*

2015/09/07 Beginnings
2015/09/18 Adding time calculation
           NOTE: This assumes that we don't skip the last frame! Otherwise the major frame
                 count is botched.
2015/09/19 Made a structure for PCM info. Ultimately it would be nice to read in a text file
                 to define PCM information.
*/


#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>

#include "parse_PCM_samples.h"
#include "TM1_majorframe.h"
#include "TM23_majorframe.h"

int main( int argc, char * argv[] )
{
    //************
    //Declare vars

    char                        szInFile[DEF_STR_SIZE];		       //input/output file stuff
    char                        szUniqCounterFile[DEF_STR_SIZE];
		                
    FILE       *                psuInFile;
    FILE       *                psuUniqCounterFile;
    		                
    char                        szOutPrefix[DEF_STR_SIZE];
		                
    struct stat                 suInFileStat;		       //input stuff
		                
    uint16_t   *                pauMinorFrame;
		                
    struct suPCMInfo	      * psuPCMInfo;   
    struct suMeasurementInfo ** ppsuMeasInfo;
    int16_t                     iMeasIdx;
		                
    //    uint16_t                    uAsymIdx;
		                
    int64_t                     llMinorFrameIdx;
    int64_t                     llOldMinorFrameIdx;

    //    uint16_t  *                 pauIsMinorFrameCollected;
    uint16_t **                 ppauMajorFrame;
    uint16_t  *                 pauMajFTemp;
		                
    uint64_t                    ullInFilePos;
    uint64_t                    ullWordsRead;
    uint64_t                    ullWordsWritten;
    uint64_t                    ullNSampsRead;
    uint64_t                    ullTotWordsRead;
    uint64_t                    ullTotWordsWritten;
		                
    int                         iArgIdx;
		                
    uint16_t                    uTMLink;
    uint8_t                     bCombineTM1Meas;
    uint8_t                     bDoCheckSFIDIncrement;
    uint8_t                     bTStampMode;
    uint8_t                     bAssembleCounter;
    uint8_t                     bWriteCounter;

    int64_t                     llWordOffset_MajorFrame;       //Current major frame offset (in words) relative to first recorded major frame
    int64_t                     llWordOffset_MinorFrame;
    int64_t                     llWordOffset_GPS;

    uint8_t                     bVerbose;
    int                         err;

    if (argc < 2)
	{
	vUsage();
	return EXIT_SUCCESS;
	}

    //Initialize vars			           
    szInFile[0]                    = '\0';
    szUniqCounterFile[0]           = '\0';

    psuInFile                      = (FILE *) NULL;
    psuUniqCounterFile             = (FILE *) NULL;

    szOutPrefix[0]                 = '\0';

    pauMinorFrame                  = NULL;

    psuPCMInfo                     = NULL;
    ppsuMeasInfo                   = NULL;
    iMeasIdx                       = 0;
    
    //    strcpy(szUniqCounterFile,"");		       // Default is stdout


    llMinorFrameIdx                = 0;
    llOldMinorFrameIdx             = 0;

    ppauMajorFrame                 = NULL;
    pauMajFTemp                    = NULL;

    ullInFilePos                   = 0;
    ullWordsRead                   = 0;
    ullWordsWritten                = 0;
    ullNSampsRead                  = 0;
    ullTotWordsRead                = 0;
    ullTotWordsWritten             = 0;

    iArgIdx                        = 0;
			           
    uTMLink                        = DEF_TM_LINK;
    bCombineTM1Meas                = DEF_COMBINE_TM1;
    bDoCheckSFIDIncrement          = DEF_DO_CHECK_SFID_INCREMENT;
    bTStampMode                    = DEF_CALC_TSTAMPS;
    bAssembleCounter               = DEF_ASSEMBLE_COUNTER;
    bWriteCounter                  = 0;

    llWordOffset_MajorFrame        = 0;
    llWordOffset_MinorFrame        = 0;
    llWordOffset_GPS               = 0;
			           
    bVerbose                       = DEF_VERBOSE;
    err                            = 0;

    for (iArgIdx=1; iArgIdx<argc; iArgIdx++)   //start with 1 to skip input file
	{

	switch (argv[iArgIdx][0])
	    {
	    int iTmp;
	    case '-' :
		switch (argv[iArgIdx][1])
		    {

		    case 'L' :                   /* TM Link # */
			iArgIdx++;
			if(iArgIdx >= argc)
			    {
			    vUsage();
			    return EXIT_FAILURE;
			    }
			sscanf(argv[iArgIdx],"%" PRIu16 ,&uTMLink);
			break;

		    case 'P' :                  /* Prefix for output files */
			iArgIdx++;
			strncpy(szOutPrefix, argv[iArgIdx],DEF_STR_SIZE);
			break;

		    case 'C' :                  /* Combine TM1 MSB/LSB measurements */
			bCombineTM1Meas       = 1;
			break;

		    case 'c' :                  /* Check that SFID increments uniformly */
			bDoCheckSFIDIncrement = 1;
			break;

		    case 'A' :                  /* Assemble and write unique counter */
			bAssembleCounter      = 1;
			bWriteCounter         = 1;
			break;

		    case 'T' :                  /* Produce timestamps based on GPS pulse */
			bTStampMode           = 1;
			bAssembleCounter      = 1; //Need counter for TStampMode
			break;

		    case 'v' :                  /* Verbosities */
			bVerbose = 1;
			break;

		    case 'h' :                  /* Verbosities */
			vUsage();
			return EXIT_SUCCESS;
			break;

		    default :
			break;
		    } /* end flag switch */
		break;

	    default :
		if (szInFile[0] == '\0') strcpy(szInFile, argv[iArgIdx]);
		break;

	    } /* end command line arg switch */
	} /* end for all arguments */

    if ( szOutPrefix[0] == '\0' )
	strncpy(szOutPrefix,DEF_OUTPREFIX,DEF_STR_SIZE);

    //init PCM channel info
    psuPCMInfo = (struct suPCMInfo * ) malloc( sizeof(struct suPCMInfo) );
    err = iPCMInit(psuPCMInfo, uTMLink, bCombineTM1Meas, bDoCheckSFIDIncrement, bTStampMode);

    if ( psuPCMInfo->ullSampBitLength < 1 || psuPCMInfo->ullSampBitLength > 16 )
	{
	fprintf(stderr,"Invalid sample size provided! Exiting...\n");
	return EXIT_FAILURE;
	}
    printf(" \n");

    //init measurements
    ppsuMeasInfo = (struct suMeasurementInfo ** ) malloc(psuPCMInfo->iNMeasurements * sizeof(struct suMeasurementInfo *));
    for ( iMeasIdx = 0; iMeasIdx < psuPCMInfo->iNMeasurements; iMeasIdx++)
	{
	ppsuMeasInfo[iMeasIdx] = (struct suMeasurementInfo *) malloc(sizeof(struct suMeasurementInfo));
	if (ppsuMeasInfo[iMeasIdx] == (struct suMeasurementInfo *) NULL)
	    {
	    printf("Couldn't initialize measurement %" PRIi16 "!\n",iMeasIdx);
	    return -1;
	    }

	err = iMeasurementInit(psuPCMInfo, ppsuMeasInfo[iMeasIdx],iMeasIdx,szOutPrefix, 
			       bCombineTM1Meas, bDoCheckSFIDIncrement, bTStampMode);
	if (bVerbose) vPrintMeasurementInfo(ppsuMeasInfo[iMeasIdx]);
	}


    //initialize major frame, binary array for keeping track of
    psuPCMInfo->ullBytesPerMinorFrame = psuPCMInfo->ullSampsPerMinorFrame * sizeof(uint16_t);
    psuPCMInfo->ullBytesPerMajorFrame = psuPCMInfo->ullBytesPerMinorFrame * psuPCMInfo->llMinorFramesPerMajorFrame;

    pauMinorFrame = malloc(psuPCMInfo->ullBytesPerMinorFrame);

    //    pauIsMinorFrameCollected = (uint16_t *) calloc(psuPCMInfo->llMinorFramesPerMajorFrame,2);
    ppauMajorFrame = (uint16_t **) malloc(psuPCMInfo->llMinorFramesPerMajorFrame * sizeof(uint16_t *));
    pauMajFTemp = malloc(psuPCMInfo->ullBytesPerMajorFrame);

    for (llMinorFrameIdx = 0; llMinorFrameIdx < psuPCMInfo->llMinorFramesPerMajorFrame; llMinorFrameIdx++) 
	{
	ppauMajorFrame[llMinorFrameIdx] = pauMajFTemp + (llMinorFrameIdx * psuPCMInfo->ullSampsPerMinorFrame);
	}
    if (bVerbose) printf("Major Frame Size   :\t%" PRIu64 " bytes\n",psuPCMInfo->ullBytesPerMajorFrame);

    int Count;

    //Open input file
    if (strlen(szInFile)==0)
	{
	vUsage();
	return EXIT_FAILURE;
	}

    psuInFile = fopen(szInFile,"rb");
    if (psuInFile == NULL)
	{
	fprintf(stderr, "Error opening input file\n");
	return EXIT_FAILURE;
	}
    printf("Input file: %s\n\n", szInFile);

    //Get input file stats
    ullInFilePos = fseek(psuInFile, 0, SEEK_SET); //go to zero, positively
    stat(szInFile, &suInFileStat);

    // If making a unique counter, open a file
    if ( bAssembleCounter && bWriteCounter )
    	{
	sprintf(szUniqCounterFile,"%s--%s.txt",szOutPrefix,"unique_counter");
    	psuUniqCounterFile = fopen(szUniqCounterFile,"w");
    	if (psuUniqCounterFile == NULL)
    	    {
    	    fprintf(stderr, "Error opening file for unique counter\n");
    	    return EXIT_FAILURE;
    	    }

    	printf("Unique counter file: %s\n", szUniqCounterFile);
    	}

    //To the beginning!
    if ( !bDoCheckSFIDIncrement ) 
	printf("Parsing PCM samples for TM%" PRIu16 "\n",psuPCMInfo->uTMLink);
    else  
	printf("Checking SFID increment; no output will be produced...\n");
    printf("\n");

    //Get first GPS word and MFCVal
    if ( !bFoundFirstMFCValAndGPSWord(psuInFile,suInFileStat.st_size, psuPCMInfo, ppsuMeasInfo, pauMinorFrame, bCombineTM1Meas, &llWordOffset_GPS) )
	{
	printf("TStampMode: Unable to get a first major frame count! Is something wrong with your data?\n");
	printf("Exiting...");
	return -1;
	}

    //Loop over whole file
    llOldMinorFrameIdx = 0;
    llMinorFrameIdx    = 1;
    do
    	{
	//reset global vars
    	ullWordsWritten                   = 0;
	psuPCMInfo->ulMinorFrameSampCount = 0;
	psuPCMInfo->uAsymWRInd            = 0;
	psuPCMInfo->uAsymFRInd            = 0;

    	ullWordsRead = fread(pauMinorFrame, 2, psuPCMInfo->ullSampsPerMinorFrame, psuInFile);     	//Get samples from infile
    	if (ullWordsRead != psuPCMInfo->ullSampsPerMinorFrame)
    	    {
    	    fprintf(stderr,"Only read %" PRIu64 " bytes of %" PRIu64 " requested!\nEOF?\n",ullWordsRead*2,psuPCMInfo->ullBytesPerMinorFrame);
    	    break;
    	    }

	//Determine which minor frame this is
	llOldMinorFrameIdx = llMinorFrameIdx;
	llMinorFrameIdx = llGetMinorFrameIdx(psuPCMInfo, pauMinorFrame);  //The TM list counts from 1, not zero
	if (bVerbose ) printf("Minor frame: %" PRIX64 "\n",llMinorFrameIdx);

	if (bDoCheckSFIDIncrement)
	    {
	    llOldMinorFrameIdx %= psuPCMInfo->llMinorFramesPerMajorFrame;

	    if ( psuPCMInfo->ullMinorFrameCount > 0 && bBadSFIDIncrement(psuPCMInfo, llMinorFrameIdx, llOldMinorFrameIdx) )
		{
		psuPCMInfo->ullSkippedFrameCount++;
		printf("Minor frame skipped!\n");
		printf("Minor frame number     : %" PRIi64 "\n",llMinorFrameIdx);
		printf("Old minor frame number : %" PRIi64 "\n",llOldMinorFrameIdx);
		printf("Net minor frame count  : %" PRIu64 "\n",psuPCMInfo->ullMinorFrameCount);
		printf("Number of mismatches   : %" PRIu64 "\n",psuPCMInfo->ullSkippedFrameCount);
		printf("\n");
		}
	    }
	else
	    {
	    for (iMeasIdx = 0; iMeasIdx < psuPCMInfo->iNMeasurements; iMeasIdx++)
		{
		    uParseMeasurementSamples(psuPCMInfo,ppsuMeasInfo[iMeasIdx],iMeasIdx,pauMinorFrame,llMinorFrameIdx,&ullWordsWritten,bCombineTM1Meas,bAssembleCounter,1);
		}
	    
	    //assemble unique counter, if requested
	    if ( bAssembleCounter )
		{
		    psuPCMInfo->ullCounterVal = ullAssembleCounterVal(psuPCMInfo,llMinorFrameIdx,&psuPCMInfo->ullCurrentMFCVal);
		    psuPCMInfo->ullCurrentMFCVal -= psuPCMInfo->ullFirstMFCVal; //Make current MF count relative to value of first recorded MF count
		    if ( bWriteCounter ) fprintf(psuUniqCounterFile,"%" PRIu64 "\n",psuPCMInfo->ullCounterVal);
		}

	    if ( bTStampMode )
	    	{
		llWordOffset_MajorFrame = ( psuPCMInfo->ullCurrentMFCVal * psuPCMInfo->llMinorFramesPerMajorFrame * psuPCMInfo->ullSampsPerMinorFrame) - 1;
		llWordOffset_MinorFrame = ( llMinorFrameIdx - 1 ) * psuPCMInfo->ullSampsPerMinorFrame;

		//Check for new GPS word in this minor frame, then calculate GPS word offset
		if ( bCheckForNewGPSWord(psuPCMInfo, ppsuMeasInfo[psuPCMInfo->pauGPSMeasIdx[0]], llMinorFrameIdx, pauMinorFrame, bCombineTM1Meas) )
		    {
			llWordOffset_GPS = llWordOffset_MajorFrame + psuPCMInfo->llCurrentGPSWord;
		    }
		
	    	for (iMeasIdx = 0; iMeasIdx < psuPCMInfo->iNMeasurements; iMeasIdx++)
	    	    {

		    //Any words that specially want attention?
		    if ( ppsuMeasInfo[iMeasIdx]->szTSSearchWord[0] != TM_NO_TS_SEARCH )
			{
			vSearchMinorFrameFor16BitWord(psuPCMInfo, ppsuMeasInfo[iMeasIdx], pauMinorFrame, llMinorFrameIdx);
			}

		    if ( ppsuMeasInfo[iMeasIdx]->bTSCalcEnabled && ( ppsuMeasInfo[iMeasIdx]->uOffsetBufCount > 0 ) )
	    		{
			iWriteMeasurementTStamps(psuPCMInfo,ppsuMeasInfo[iMeasIdx], llMinorFrameIdx,
						 llWordOffset_MajorFrame, llWordOffset_MinorFrame,
						 llWordOffset_GPS);
	    		}
	    	    }
	    	}
	    }

	//did we get the whole major frame?
	psuPCMInfo->ullMinorFrameCount++;
	if ( llMinorFrameIdx == psuPCMInfo->llMinorFramesPerMajorFrame ) psuPCMInfo->llMajorFrameCount++; //NOTE: This assumes that we don't skip the last frame!
	if ( bVerbose ) printf("Major frame #%" PRIu64 "\n", psuPCMInfo->llMajorFrameCount);

    	if (bVerbose) 
    	    {
    	    printf("N InSamps read     : %" PRIu64 "\n",ullNSampsRead);
    	    printf("InFile Position (bytes) : %" PRIu64 "\n",ullInFilePos);
    	    printf("Read %" PRIu64 " bytes; Wrote %" PRIu64 " bytes\n",ullWordsRead*2,ullWordsWritten);
    	    }

    	ullTotWordsRead += ullWordsRead;
    	ullTotWordsWritten += ullWordsWritten;

    	}  while( ( ullInFilePos = ftell(psuInFile) ) < suInFileStat.st_size  );
    
    //Summary
    printf("Minor frame count           : %" PRIu64 "\n", psuPCMInfo->ullMinorFrameCount);
    printf("Major frame count           : %" PRIu64 "\n", psuPCMInfo->llMajorFrameCount);
    if (bDoCheckSFIDIncrement) 
	{
	printf("Total number of frame skips : %" PRIu64 "\n",psuPCMInfo->ullSkippedFrameCount);
	if ( psuPCMInfo->ullSkippedFrameCount == 0 )
	    printf("\nShe's clean! No frame skips detected\n");
	else
	    printf("\nFrame skips exist within this file.\n");
	}
    else
	{
	printf("\n");
	printf("Wrote %" PRIu64 "./%" PRIu64 ". (Bytes / Total Bytes) \n", ullTotWordsWritten*2, (suInFileStat.st_size));
	printf("Wrote %" PRIu64 "./%" PRIu64 ". (Bytes / Total Bytes) \n", ullTotWordsWritten*2, (suInFileStat.st_size));
	printf("Output file size is %.2f%% of input file\n", (float)((float)( ullTotWordsWritten*2 )/(float)(suInFileStat.st_size))*100);
	}
    
    fclose(psuInFile); //close input file

    //release the mem!
    iPCMFree(psuPCMInfo);

    for ( iMeasIdx = 0; iMeasIdx < psuPCMInfo->iNMeasurements; iMeasIdx++)
	{
	if ( ppsuMeasInfo[iMeasIdx] != NULL ) iMeasurementFree(ppsuMeasInfo[iMeasIdx]);
	}
    free(ppsuMeasInfo);

    //junk mem for major frame
    if ( pauMajFTemp                != NULL ) free(pauMajFTemp);
    if ( ppauMajorFrame             != NULL ) free(ppauMajorFrame);

    //junk minor frame stuff
    if ( pauMinorFrame              != NULL ) free(pauMinorFrame);
    //    if ( pauIsMinorFrameCollected   != NULL ) free(pauIsMinorFrameCollected);

    //Extras that should go away
    if ( psuUniqCounterFile         != NULL ) fclose(psuUniqCounterFile);

    return EXIT_SUCCESS;
}

int iPCMInit(struct suPCMInfo * psuPCMInfo, uint16_t uTMLink, uint8_t bCombineTM1Meas, uint8_t bDoCheckSFIDIncrement, uint8_t bTStampMode )
{
    int iArgIdx;

    //*******************
    //Initialize vars
    psuPCMInfo->uAsymWRInd                    = 0;
    psuPCMInfo->uAsymFRInd                    = 0;
    psuPCMInfo->uSFIDIdx                      = 0;
    psuPCMInfo->uTMLink                       = uTMLink;

    psuPCMInfo->ulMinorFrameSampCount          = 0;
    psuPCMInfo->ullMinorFrameCount             = 0;
    psuPCMInfo->ullSkippedFrameCount           = 0;
    psuPCMInfo->llMajorFrameCount              = 0;

    psuPCMInfo->pauMFCIndices                  = NULL;
    psuPCMInfo->paullMajorFrameVals            = NULL;
    psuPCMInfo->ullFirstMFCVal                 = 0;
    psuPCMInfo->ullCurrentMFCVal               = 0;
    psuPCMInfo->ullGPSMFCVal                   = 0;

    psuPCMInfo->ullCounterVal                  = 0;
    psuPCMInfo->pauGPSMeasIdx                  = NULL;
    psuPCMInfo->llCurrentGPSWord               = -1;
    psuPCMInfo->ullGPSWordCount                = -1;     //This will go to zero once we initialize the GPS word and MFC count
    psuPCMInfo->ullGPSWordStreakCount          = 0;

    psuPCMInfo->bDoSearchFrameForWord          = 0;

    if ( psuPCMInfo->uTMLink == 1 )
	{
	printf("TM1 : SKIN/ELF/VF/VLF/AGC samples\n");

	psuPCMInfo->iNMeasurements                   = N_TM1_MEASUREMENTS;
        psuPCMInfo->uSFIDIdx                        = TM1_SFID_IDX;
	psuPCMInfo->ullSampsPerMinorFrame           = TM1_SAMPSPERMINORFRAME;
	psuPCMInfo->ullBytesPerMinorFrame            = psuPCMInfo->ullSampsPerMinorFrame * sizeof(uint16_t);
	psuPCMInfo->llMinorFramesPerMajorFrame      = TM1_MINOR_PER_MAJOR;
	psuPCMInfo->uNumMFCounters                   = TM1_NUM_MFCOUNTERS;
	psuPCMInfo->paullMajorFrameVals              = (uint64_t *) malloc(psuPCMInfo->uNumMFCounters * sizeof(uint64_t));
	psuPCMInfo->pauMFCIndices                    = (uint16_t *) malloc(psuPCMInfo->uNumMFCounters * sizeof(uint16_t));
	for ( iArgIdx = 0; iArgIdx < psuPCMInfo->uNumMFCounters; iArgIdx++ )
	    {			         
	    psuPCMInfo->pauMFCIndices[iArgIdx]       = auTM1MFCMeasIdx[iArgIdx];
	    psuPCMInfo->paullMajorFrameVals[iArgIdx] = -1;
	    }			         
				         
	psuPCMInfo->ullSampBitLength                 = TM1_WORD_BITLENGTH;
	psuPCMInfo->uMinorFrameBitShift              = TM1_MINORFRAME_BITSHIFT;
	psuPCMInfo->ullBitRate                       = TM1_BPS;
	psuPCMInfo->dWordPeriod                     = (double) psuPCMInfo->ullSampBitLength/ (double) psuPCMInfo->ullBitRate;
			                   
	if ( bTStampMode )               
	    {		                   
	    psuPCMInfo->uNGPSWordsInPCM              = TM1_N_GPS_WORDS;
	    psuPCMInfo->pauGPSMeasIdx                = (uint16_t *) malloc(psuPCMInfo->uNGPSWordsInPCM * sizeof(uint16_t));
	    for ( iArgIdx = 0; iArgIdx < psuPCMInfo->uNGPSWordsInPCM; iArgIdx++ )
		{		         
		psuPCMInfo->pauGPSMeasIdx[iArgIdx]   = auTM1GPSMeasIdx[iArgIdx];
		}
	    }

	if ( bCombineTM1Meas ) printf("Combining MSB/LSB measurements on TM1!\n");

	}
    else
	{
	printf("TM2/3: RxDSP samples\n");

	psuPCMInfo->iNMeasurements                   = N_TM23_MEASUREMENTS;
        psuPCMInfo->uSFIDIdx                        = TM23_SFID_IDX;
	psuPCMInfo->ullSampsPerMinorFrame           = TM23_SAMPSPERMINORFRAME;
	psuPCMInfo->ullBytesPerMinorFrame            = psuPCMInfo->ullSampsPerMinorFrame * sizeof(uint16_t);
	psuPCMInfo->llMinorFramesPerMajorFrame      = TM23_MINOR_PER_MAJOR;				         
	psuPCMInfo->uNumMFCounters                   = TM23_NUM_MFCOUNTERS;
	psuPCMInfo->paullMajorFrameVals              = (uint64_t *) malloc(psuPCMInfo->uNumMFCounters * sizeof(uint64_t));
	psuPCMInfo->pauMFCIndices                    = (uint16_t *) malloc(psuPCMInfo->uNumMFCounters * sizeof(uint16_t));
	for ( iArgIdx = 0; iArgIdx < psuPCMInfo->uNumMFCounters; iArgIdx++ )
	    {			         
	    psuPCMInfo->pauMFCIndices[iArgIdx]       = auTM23MFCMeasIdx[iArgIdx];
	    psuPCMInfo->paullMajorFrameVals[iArgIdx] = -1;
	    }			         
				         
	psuPCMInfo->ullSampBitLength                 = TM23_WORD_BITLENGTH;
	psuPCMInfo->uMinorFrameBitShift              = TM23_MINORFRAME_BITSHIFT;
	psuPCMInfo->ullBitRate                       = TM23_BPS;
	psuPCMInfo->dWordPeriod                     = (double) psuPCMInfo->ullSampBitLength/(double) psuPCMInfo->ullBitRate;
				         
	if ( bTStampMode )               
	    {		                   
	    psuPCMInfo->uNGPSWordsInPCM              = TM23_N_GPS_WORDS;
	    psuPCMInfo->pauGPSMeasIdx                = (uint16_t *) malloc(psuPCMInfo->uNGPSWordsInPCM * sizeof(uint16_t));
	    for ( iArgIdx = 0; iArgIdx < psuPCMInfo->uNGPSWordsInPCM; iArgIdx++ )
		{		         
		psuPCMInfo->pauGPSMeasIdx[iArgIdx]   = auTM23GPSMeasIdx[iArgIdx];
		}		         
	    }			         


	}
}

int iMeasurementInit(struct suPCMInfo * psuPCMInfo, struct suMeasurementInfo * psuMeasInfo, int16_t iMeasIdx,char * szOutPrefix, 
		     uint8_t bCombineTM1Meas, uint8_t bDoCheckSFIDIncrement, uint8_t bTStampMode )
{
    uint16_t uNAsymWRanges;
    uint16_t uNAsymFRanges;

    uint64_t ullBytesInAsymWRanges;
    uint64_t ullBytesInAsymFRanges;

    uint64_t ullAsymRangeIdx;

    uint16_t uWRangeIdx;
    uint16_t uFRangeIdx;

    size_t   szTempLen;
    size_t   szAbbrevLen;

    int iTmpIdx;

    psuMeasInfo->szAbbrev[0]       = '\0';
    psuMeasInfo->szOutFile[0]      = '\0';
    psuMeasInfo->szTStampFile[0]   = '\0';

    if ( psuPCMInfo->uTMLink == 1)
	{

	szTempLen = 0;
	while ( szTM1SerialMeasNames[iMeasIdx][szTempLen] != '\0' )
	    szTempLen++;
	strncpy(psuMeasInfo->szName, szTM1SerialMeasNames[iMeasIdx],	                   //Name of measurement, e.g., "Langmuir Probe Measurement 1 MSB"
		szTempLen );    

	szAbbrevLen = 0;
	while ( szTM1SerialMeasAbbrev[iMeasIdx][szAbbrevLen] != '\0' )
		szAbbrevLen++;
	strncpy(psuMeasInfo->szAbbrev, szTM1SerialMeasAbbrev[iMeasIdx],			   //Abbreviation for measurement, e.g., "LP01MSB"
		szAbbrevLen );

	szTempLen = 0;
	while ( szTM1User[iMeasIdx][szTempLen] != '\0' )
		szTempLen++;
        strncpy(psuMeasInfo->szUser, szTM1User[iMeasIdx],				   //Who is the user? E.g., Dartmouth, U Iowa
		szTempLen );

        psuMeasInfo->uWord               = uTM1Word[iMeasIdx] - 1;	                   //Beginning word in the frame, -1 for counting from zero
        psuMeasInfo->uWdInt              = uTM1WdInt[iMeasIdx];				   //Word interval
        psuMeasInfo->uMinorFrame         = uTM1Frame[iMeasIdx];				   //Which minor frame is it in?
        psuMeasInfo->uMinorFrInt         = uTM1FrInt[iMeasIdx];				   //How often does it show up?
        psuMeasInfo->ulSPS               = ulTM1SPS[iMeasIdx];
        			         
        psuMeasInfo->uSampsPerMinorFrame = psuPCMInfo->ullSampsPerMinorFrame/psuMeasInfo->uWdInt;      //How many of these to expect per frame? Most are just one.

	psuMeasInfo->uLSBWord            = uTM1LSBWord[iMeasIdx] - 1;
	if ( ( bCombineTM1Meas && ( psuMeasInfo->uLSBWord != TM_SKIP_LSB - 1 ) ) && ( psuMeasInfo->uLSBWord != TM_NO_LSB -1 ) )
	    psuMeasInfo->szAbbrev[szAbbrevLen-4] = '\0';

        uNAsymWRanges                    = uTM1NAsymWRanges[iMeasIdx];
        uNAsymFRanges                    = uTM1NAsymFRanges[iMeasIdx];

	if ( bTStampMode )
	    {
	    psuMeasInfo->bTSCalcEnabled  = abTM1TSCalcEnabled[iMeasIdx];
	    sprintf(psuMeasInfo->szTSSearchWord,"%s",aszTM1TSSearchWords[iMeasIdx]); 
	    if ( psuMeasInfo->szTSSearchWord != TM_NO_TS_SEARCH )
		{
		psuPCMInfo->bDoSearchFrameForWord = 1;
		}
	    }
	else
	    {
	    psuMeasInfo->bTSCalcEnabled  = 0;
	    }
	}
    else if ( ( psuPCMInfo->uTMLink == 2 ) || ( psuPCMInfo->uTMLink == 3 ) )
	{
	strncpy(psuMeasInfo->szName, szTM23SerialMeasNames[iMeasIdx],	                   //Name of measurement, e.g., "Langmuir Probe Measurement 1 MSB"
		strlen(szTM23SerialMeasNames[iMeasIdx]) );    
	strncpy(psuMeasInfo->szAbbrev, szTM23SerialMeasAbbrev[iMeasIdx],		   //Abbreviation for measurement, e.g., "LP01MSB"
		strlen(szTM23SerialMeasAbbrev[iMeasIdx]) );
        strncpy(psuMeasInfo->szUser, szTM23User[iMeasIdx],				   //Who is the user? E.g., Dartmouth, U Iowa
		strlen(szTM23User[iMeasIdx]) );
        psuMeasInfo->uWord               = uTM23Word[iMeasIdx] - 1;			   //Beginning word in the frame, -1 for counting from zero
        psuMeasInfo->uWdInt              = uTM23WdInt[iMeasIdx];			   //Word interval
        psuMeasInfo->uMinorFrame         = uTM23Frame[iMeasIdx];			   //Which minor frame is it in?
        psuMeasInfo->uMinorFrInt         = uTM23FrInt[iMeasIdx];			   //How often does it show up?
        psuMeasInfo->ulSPS               = ulTM23SPS[iMeasIdx];
        
	//        psuMeasInfo->uSample             = -1;
        psuMeasInfo->ullSampCount        = 0;
        			         
        psuMeasInfo->uSampsPerMinorFrame = psuPCMInfo->ullSampsPerMinorFrame/psuMeasInfo->uWdInt;      //How many of these to expect per frame? Most are just one.

	psuMeasInfo->uLSBWord            = uTM23LSBWord[iMeasIdx] - 1;

        uNAsymWRanges                    = uTM23NAsymWRanges[iMeasIdx];
        uNAsymFRanges                    = uTM23NAsymFRanges[iMeasIdx];

	if ( bTStampMode )
	    {
	    psuMeasInfo->bTSCalcEnabled  = abTM23TSCalcEnabled[iMeasIdx];
	    if ( aszTM23TSSearchWords[iMeasIdx] != '\0' )
		sprintf(psuMeasInfo->szTSSearchWord,"%s",aszTM23TSSearchWords[iMeasIdx]); 
	    else
		psuMeasInfo->szTSSearchWord[0] = '\0';
	    }
	else
	    {
	    psuMeasInfo->bTSCalcEnabled  = 0;
	    }

	}

	//handle asymmetric word ranges
    if (uNAsymWRanges > 0)
	{
	psuMeasInfo->uNAsymWRanges       = uNAsymWRanges;
	psuMeasInfo->ppauAsymWRanges     = (uint16_t **) malloc(uNAsymWRanges * sizeof(uint16_t *));
	
	ullBytesInAsymWRanges            = uNAsymWRanges * sizeof(uint16_t *) * 2;
	
	psuMeasInfo->uSampsPerMinorFrame = 0; //This has to be recalculated

	//initialize memory word ranges
	psuMeasInfo->pauWtemp            = malloc(uNAsymWRanges * 2 * sizeof(uint16_t));
	for (ullAsymRangeIdx = 0; ullAsymRangeIdx < uNAsymWRanges; ullAsymRangeIdx++) 
	    {
	    psuMeasInfo->ppauAsymWRanges[ullAsymRangeIdx] = psuMeasInfo->pauWtemp + (ullAsymRangeIdx * 2);
	    }
	
	//Now assign the values
	for (uWRangeIdx = 0; uWRangeIdx < uNAsymWRanges; uWRangeIdx++)
	    {
	    if ( psuPCMInfo->uTMLink == 1)
		{
		psuMeasInfo->ppauAsymWRanges[uWRangeIdx][0] = uTM1AsymWRanges[psuPCMInfo->uAsymWRInd][0];
		psuMeasInfo->ppauAsymWRanges[uWRangeIdx][1] = uTM1AsymWRanges[psuPCMInfo->uAsymWRInd][1];
		}
	    else if ( ( psuPCMInfo->uTMLink == 2 ) || ( psuPCMInfo->uTMLink == 3 ) )
		{
		psuMeasInfo->ppauAsymWRanges[uWRangeIdx][0] = uTM23AsymWRanges[psuPCMInfo->uAsymWRInd][0];
		psuMeasInfo->ppauAsymWRanges[uWRangeIdx][1] = uTM23AsymWRanges[psuPCMInfo->uAsymWRInd][1];
		}
	    psuMeasInfo->uSampsPerMinorFrame += (psuMeasInfo->ppauAsymWRanges[uWRangeIdx][1] - psuMeasInfo->ppauAsymWRanges[uWRangeIdx][0] + 1);
	    psuPCMInfo->uAsymWRInd++;
	    }
	}
    else
	{
	psuMeasInfo->uNAsymWRanges   = 0;
	psuMeasInfo->ppauAsymWRanges = NULL;
	psuMeasInfo->pauWtemp        = NULL;
	}
    
    //handle asymmetric frame ranges
    if (uNAsymFRanges > 0)
	{
        psuMeasInfo->uNAsymFRanges   = uNAsymFRanges;
	psuMeasInfo->ppauAsymFRanges = (uint16_t **) malloc(uNAsymFRanges * sizeof(uint16_t *));
	
	ullBytesInAsymFRanges      = uNAsymFRanges * sizeof(uint16_t *) * 2;
	
	//initialize memory word ranges
	psuMeasInfo->pauFtemp = malloc(uNAsymFRanges * 2 * sizeof(uint16_t));
	for (ullAsymRangeIdx = 0; ullAsymRangeIdx < uNAsymFRanges; ullAsymRangeIdx++) 
	    {
	    psuMeasInfo->ppauAsymFRanges[ullAsymRangeIdx] = psuMeasInfo->pauFtemp + (ullAsymRangeIdx * 2);
	    }
	
	//Now assign the values
	for (uFRangeIdx = 0; uFRangeIdx < uNAsymFRanges; uFRangeIdx++)
	    {
	    if ( psuPCMInfo->uTMLink == 1)
		{
		psuMeasInfo->ppauAsymFRanges[uFRangeIdx][0] = uTM1AsymFRanges[psuPCMInfo->uAsymFRInd][0];
		psuMeasInfo->ppauAsymFRanges[uFRangeIdx][1] = uTM1AsymFRanges[psuPCMInfo->uAsymFRInd][1];
		}
	    else if ( ( psuPCMInfo->uTMLink == 2 ) || ( psuPCMInfo->uTMLink == 3 ) )
		{
		psuMeasInfo->ppauAsymFRanges[uFRangeIdx][0] = uTM23AsymFRanges[psuPCMInfo->uAsymFRInd][0];
		psuMeasInfo->ppauAsymFRanges[uFRangeIdx][1] = uTM23AsymFRanges[psuPCMInfo->uAsymFRInd][1];
		}
	    psuPCMInfo->uAsymFRInd++;
	    }
	}
    else
	{
	psuMeasInfo->uNAsymFRanges   = 0;
	psuMeasInfo->ppauAsymFRanges = NULL;
	psuMeasInfo->pauFtemp        = NULL;
	}
    
    //Now handle initialization of buffers
    psuMeasInfo->palSample           = (int32_t *) malloc(psuMeasInfo->uSampsPerMinorFrame * sizeof(int32_t));
    for ( iTmpIdx = 0; iTmpIdx < psuMeasInfo->uSampsPerMinorFrame; iTmpIdx++ )
	psuMeasInfo->palSample[iTmpIdx] = -1;


    if ( ( ( psuPCMInfo->uTMLink > 1 ) || ( ( psuMeasInfo->uLSBWord  != TM_SKIP_LSB - 1 ) || !bCombineTM1Meas ) ) && !bDoCheckSFIDIncrement )
	{
	sprintf(psuMeasInfo->szOutFile,"%s--%s.out",szOutPrefix,psuMeasInfo->szAbbrev);
	psuMeasInfo->psuOutFile       = (FILE *) fopen(psuMeasInfo->szOutFile,"wb");
	}
    else
	{
	psuMeasInfo->szOutFile[0]     = '\0';
	psuMeasInfo->psuOutFile       = NULL;
	}

    if ( psuMeasInfo->bTSCalcEnabled )
	{
	sprintf(psuMeasInfo->szTStampFile,"%s--%s--tstamps.txt",szOutPrefix,psuMeasInfo->szAbbrev);
	psuMeasInfo->psuTStampFile    = (FILE *) fopen(psuMeasInfo->szTStampFile,"w");
	psuMeasInfo->pallWordOffsets  = (int64_t *) malloc(psuMeasInfo->uSampsPerMinorFrame * sizeof(int64_t));
	}
    else
	{
	psuMeasInfo->szTStampFile[0]  = '\0';
	psuMeasInfo->psuTStampFile    = NULL;
	psuMeasInfo->pallWordOffsets  = NULL;
	}
	psuMeasInfo->uOffsetBufCount  = 0;

    return 0;
}


//write samples from this minor frame to the appropriate measurement files
uint16_t uParseMeasurementSamples(struct suPCMInfo * psuPCMInfo, struct suMeasurementInfo * psuMeasInfo, int iMeasIdx, 
				  uint16_t * pauMinorFrame, int64_t llMinorFrameIdx, 
				  uint64_t * pullWordsWritten,
				  uint8_t bCombineTM1Meas, uint8_t bAssembleCounter, uint8_t bWriteSamplesToFile)
{
    int iArgIdx;
    int iWdIdx;
    int iFrIdx;

    uint16_t uAsymIdx;
    // If there are no asymmetric word or frame ranges, just write the sample at the specified word and any others within minor frame
    if ( ( psuMeasInfo->uNAsymWRanges == 0 ) && ( psuMeasInfo->uNAsymFRanges == 0 ) )
	{
	if ( (llMinorFrameIdx % psuMeasInfo->uMinorFrInt) == ( psuMeasInfo->uMinorFrame % psuMeasInfo->uMinorFrInt ) )
	    {
	    if ( ( psuPCMInfo->uTMLink == 1 ) && ( bCombineTM1Meas ) )
		{
		int iLSBWdIdx;
		uint16_t uCombinedSample;
		iLSBWdIdx = psuMeasInfo->uLSBWord;
		if( iLSBWdIdx != ( TM_SKIP_LSB - 1 ) )
		    {
		    for (iWdIdx = psuMeasInfo->uWord; iWdIdx < psuPCMInfo->ullSampsPerMinorFrame; iWdIdx += psuMeasInfo->uWdInt)
			{
			if ( iLSBWdIdx == TM_NO_LSB - 1 )
			    {
			    uCombinedSample = pauMinorFrame[iWdIdx];
			    if ( bAssembleCounter )
				{
				for ( iArgIdx = 0; iArgIdx < psuPCMInfo->uNumMFCounters; iArgIdx++ )
				    {
				    if ( iMeasIdx == psuPCMInfo->pauMFCIndices[iArgIdx] )
					{
					psuPCMInfo->paullMajorFrameVals[iArgIdx] = uCombinedSample;
					break;
					}
				    }
				}
			    }
			else if ( iLSBWdIdx == TM_UPPER6_MSB_LOWER10_LSB - 1) //GPS and ACS are weird
			    {
			    uCombinedSample = ( ( pauMinorFrame[iWdIdx] & 0x3FF ) << 10 ) | ( pauMinorFrame[iLSBWdIdx] );
			    }
			else
			    {
			    uCombinedSample = ( ( pauMinorFrame[iWdIdx] & 0x3FF ) <<  6 ) | ( pauMinorFrame[iLSBWdIdx] >> 4 );
			    }
			
			if ( bWriteSamplesToFile )
			    {
			    (*pullWordsWritten) += fwrite(&uCombinedSample,2,1,psuMeasInfo->psuOutFile) * 2;
			    psuMeasInfo->ullSampCount++;
			    psuPCMInfo->ulMinorFrameSampCount += 2;
			    if ( psuMeasInfo->bTSCalcEnabled && ( psuMeasInfo->szTSSearchWord == TM_NO_TS_SEARCH ) ) 
				{
				psuMeasInfo->pallWordOffsets[psuMeasInfo->uOffsetBufCount] = iWdIdx;
				psuMeasInfo->uOffsetBufCount++;
				}
			    }
			iLSBWdIdx += psuMeasInfo->uWdInt;
			}
		    }
		}
	    else
		{
		for (iWdIdx = psuMeasInfo->uWord; iWdIdx < psuPCMInfo->ullSampsPerMinorFrame; iWdIdx += psuMeasInfo->uWdInt)
		    {
		    if ( bWriteSamplesToFile )
			{
			(*pullWordsWritten) += fwrite(&pauMinorFrame[iWdIdx],2,1,psuMeasInfo->psuOutFile);
			psuMeasInfo->ullSampCount++;
			psuPCMInfo->ulMinorFrameSampCount++;
			if ( psuMeasInfo->bTSCalcEnabled && ( psuMeasInfo->szTSSearchWord == TM_NO_TS_SEARCH ) ) 
			    {
			    psuMeasInfo->pallWordOffsets[psuMeasInfo->uOffsetBufCount] = iWdIdx;
			    psuMeasInfo->uOffsetBufCount++;
			    }
			}
		    if ( bAssembleCounter )
			{
			for ( iArgIdx = 0; iArgIdx < psuPCMInfo->uNumMFCounters; iArgIdx++ )
			    {
			    if ( iMeasIdx == psuPCMInfo->pauMFCIndices[iArgIdx] )
				{
				psuPCMInfo->paullMajorFrameVals[iArgIdx] = pauMinorFrame[iWdIdx];
				break;
				}
			    }
			}
		    }
		}
	    
	    }
	}
    // Otherwise, if we have asymmetric word ranges but no asymmetric frame ranges ...
    else if ( ( psuMeasInfo->uNAsymWRanges > 0 ) && ( psuMeasInfo->uNAsymFRanges == 0 ) )
	{
	int iLowerLim;
	int iUpperLim;
	for (uAsymIdx = 0; uAsymIdx < psuMeasInfo->uNAsymWRanges; uAsymIdx++)
	    {
	    iLowerLim = psuMeasInfo->ppauAsymWRanges[uAsymIdx][0]; //these ranges are inclusive
	    iUpperLim = psuMeasInfo->ppauAsymWRanges[uAsymIdx][1];
	    for ( iWdIdx = iLowerLim; iWdIdx <= iUpperLim; iWdIdx++)
		{
		if ( bWriteSamplesToFile )
		    {
		    (*pullWordsWritten) += fwrite(&pauMinorFrame[iWdIdx],2,1,psuMeasInfo->psuOutFile);
		    psuMeasInfo->ullSampCount++;
		    psuPCMInfo->ulMinorFrameSampCount++;
		    if ( psuMeasInfo->bTSCalcEnabled && ( psuMeasInfo->szTSSearchWord == TM_NO_TS_SEARCH ) ) 
			{
			psuMeasInfo->pallWordOffsets[psuMeasInfo->uOffsetBufCount] = iWdIdx;
			psuMeasInfo->uOffsetBufCount++;
			}
		    }
		}
	    }
	psuPCMInfo->uAsymWRInd += psuMeasInfo->uNAsymWRanges;
	}
    // Otherwise, if we have asymmetric frame ranges but no asymmetric word ranges ...
    else if ( ( psuMeasInfo->uNAsymWRanges == 0 ) && ( psuMeasInfo->uNAsymFRanges > 0 ) )
	{
	int iLowerLim;
	int iUpperLim;
	for (uAsymIdx = 0; uAsymIdx < psuMeasInfo->uNAsymFRanges; uAsymIdx++)
	    {
	    iLowerLim = psuMeasInfo->ppauAsymFRanges[uAsymIdx][0]; //these ranges are inclusive
	    iUpperLim = psuMeasInfo->ppauAsymFRanges[uAsymIdx][1];
	    for ( iFrIdx = iLowerLim; iFrIdx <= iUpperLim; iFrIdx++)
		{
		if ( iFrIdx == llMinorFrameIdx )
		    {
		    if ( bWriteSamplesToFile )
			{
			(*pullWordsWritten) += fwrite(&pauMinorFrame[psuMeasInfo->uWord],2,1,psuMeasInfo->psuOutFile);
			psuMeasInfo->ullSampCount++;
			psuPCMInfo->ulMinorFrameSampCount++;
			if ( psuMeasInfo->bTSCalcEnabled && ( psuMeasInfo->szTSSearchWord == TM_NO_TS_SEARCH ) ) 
			    {
			    psuMeasInfo->pallWordOffsets[psuMeasInfo->uOffsetBufCount] = iWdIdx;
			    psuMeasInfo->uOffsetBufCount++;
			    }
			}
		    if ( bAssembleCounter )
			{
			for ( iArgIdx = 0; iArgIdx < psuPCMInfo->uNumMFCounters; iArgIdx++ )
			    {
			    if ( iMeasIdx == psuPCMInfo->pauMFCIndices[iArgIdx] )
				{
				psuPCMInfo->paullMajorFrameVals[iArgIdx] = pauMinorFrame[psuMeasInfo->uWord];
				break;
				}
			    }
			}
		    }
		}
	    }
	psuPCMInfo->uAsymFRInd += psuMeasInfo->uNAsymFRanges;
	}

    return 0;
}

/*For combining samples. Clearly unfinished*/
uint16_t combine_MSB_LSB_sample(uint16_t uMSBSample, uint16_t uLSBSample, 
				uint16_t uMSBShift, uint16_t uLSBShift, 
				uint16_t uJustification, uint8_t bMSBIsFirst)
{
    uint16_t uCombinedSample;

    uCombinedSample = ( uMSBSample << uMSBShift );

    return uCombinedSample;
}

uint64_t ullAssembleCounterVal(struct suPCMInfo * psuPCMInfo, int64_t llMinorFrameIdx,uint64_t * pullMFCVal)
{
    uint64_t ullCounterVal;
    int      iMFCIdx;

    ullCounterVal = 0;
    for ( iMFCIdx = 0; iMFCIdx < psuPCMInfo->uNumMFCounters; iMFCIdx++ )
	{
	    ullCounterVal = ullCounterVal | ( psuPCMInfo->paullMajorFrameVals[iMFCIdx] << psuPCMInfo->ullSampBitLength*iMFCIdx ); 
	}
    if ( pullMFCVal != NULL ) 
	(*pullMFCVal) = ullCounterVal;

	ullCounterVal = (ullCounterVal << psuPCMInfo->uMinorFrameBitShift) | (llMinorFrameIdx - 1); //last, tack on minor frame

    return ullCounterVal;
}

uint8_t bFoundFirstMFCValAndGPSWord(FILE * psuInFile, size_t szInFileSize, 
				    struct suPCMInfo * psuPCMInfo, struct suMeasurementInfo ** ppsuMeasInfo,
				     uint16_t * pauMinorFrame, uint8_t bCombineTM1Meas, int64_t * pllWordOffset_GPS)
{
    int                  iMeasIdx;
    int                  iArgIdx;

    uint64_t             ullWordsWritten;
    uint32_t             ulMinorFrameSampCount;
    int64_t              llMinorFrameIdx;
    uint64_t             ullInFilePos;
    uint64_t             ullTemp;
    uint16_t             uMFCValCount;
    uint8_t              bAllMFCValsCollected;

    uint8_t              bGotFirstGPSWord;
    int64_t              llFirstGPSWord;

    ullWordsWritten      = 0;
    ulMinorFrameSampCount= 0;
    llMinorFrameIdx      = 0;
    ullInFilePos         = 0;
    iMeasIdx             = 0;
    ullTemp              = 0;
    uMFCValCount         = 0;
    bAllMFCValsCollected = 0;

    bGotFirstGPSWord     = 0;
    llFirstGPSWord       = 0;
	
    while ( ( !bAllMFCValsCollected || !bGotFirstGPSWord ) && ( ( ullInFilePos = ftell(psuInFile) ) < szInFileSize ) )
	{
	psuPCMInfo->uAsymWRInd = 0;
	psuPCMInfo->uAsymFRInd = 0;

	ullTemp = fread(pauMinorFrame, 2, psuPCMInfo->ullSampsPerMinorFrame, psuInFile);
	if (ullTemp != psuPCMInfo->ullSampsPerMinorFrame)
	    {
	    fprintf(stderr,"Only read %" PRIu64 " bytes of %" PRIu64 " requested!\nEOF?\n",ullTemp*2,psuPCMInfo->ullSampsPerMinorFrame*2);
	    return -1;
	    }
	
	llMinorFrameIdx = llGetMinorFrameIdx(psuPCMInfo, pauMinorFrame);  //The TM list counts from 1, not zero
	
	for (iMeasIdx = 0; iMeasIdx < psuPCMInfo->iNMeasurements; iMeasIdx++)
	    {
		uParseMeasurementSamples(psuPCMInfo, ppsuMeasInfo[iMeasIdx], iMeasIdx, pauMinorFrame, llMinorFrameIdx, &ullWordsWritten, bCombineTM1Meas,1,0);
	    }
	
	if ( !bAllMFCValsCollected )
	    {
		for ( iArgIdx = 0; iArgIdx < psuPCMInfo->uNumMFCounters; iArgIdx++ )
		    {			         
		    if ( psuPCMInfo->paullMajorFrameVals[iArgIdx] != -1 )
			uMFCValCount++;
		    }		
	    }
	
	
	if ( uMFCValCount == psuPCMInfo->uNumMFCounters )
	    {
		psuPCMInfo->ullCounterVal = ullAssembleCounterVal(psuPCMInfo,llMinorFrameIdx, &psuPCMInfo->ullFirstMFCVal);
	    bAllMFCValsCollected = 1;
	    uMFCValCount = 0;
	    }

	if ( !bGotFirstGPSWord )
	    {
		if ( bCheckForNewGPSWord(psuPCMInfo, ppsuMeasInfo[psuPCMInfo->pauGPSMeasIdx[0]], llMinorFrameIdx, pauMinorFrame, bCombineTM1Meas) )
		{
		bGotFirstGPSWord = 1;
		psuPCMInfo->ullCounterVal = ullAssembleCounterVal(psuPCMInfo, llMinorFrameIdx,&psuPCMInfo->ullGPSMFCVal);
		}
	    }
	}

    if ( bAllMFCValsCollected && bGotFirstGPSWord )
	{
	//Get the proper GPS offset
	(*pllWordOffset_GPS) = psuPCMInfo->llMinorFramesPerMajorFrame * (psuPCMInfo->ullGPSMFCVal - psuPCMInfo->ullFirstMFCVal) * psuPCMInfo->ullSampsPerMinorFrame;  

	//Now we go back to the beginning of the file
	rewind(psuInFile);
	ullInFilePos = 0;
	}
    else
	{
	printf("allmfc: %i, gotfirstgps: %i\n",bAllMFCValsCollected,bGotFirstGPSWord);
	return 0;
	}

    return 1;
}

int bCheckForNewGPSWord(struct suPCMInfo * psuPCMInfo, struct suMeasurementInfo * psuMeasInfo, int64_t llMinorFrameIdx, uint16_t * pauMinorFrame,
			uint8_t bCombineTM1Meas)
{
    int iWdIdx;

    uint16_t uNonZeroGPSWordCount;
    int64_t  llTestGPSWord;

    uNonZeroGPSWordCount = 0;
    llTestGPSWord        = -1;

    if ( (llMinorFrameIdx % psuMeasInfo->uMinorFrInt) == ( psuMeasInfo->uMinorFrame % psuMeasInfo->uMinorFrInt ) )
	{
	if ( ( psuPCMInfo->uTMLink == 1 ) && ( bCombineTM1Meas ) )
	    {
	    int iLSBWdIdx;
	    iWdIdx = psuMeasInfo->uWord;
	    iLSBWdIdx = psuMeasInfo->uLSBWord;

	    if( iLSBWdIdx != ( TM_SKIP_LSB - 1 ) )
		{
		if ( iLSBWdIdx == TM_NO_LSB - 1 )
		    {
		    llTestGPSWord = pauMinorFrame[iWdIdx];
		    }
		else if ( iLSBWdIdx == TM_UPPER6_MSB_LOWER10_LSB - 1) //GPS and ACS are weird
		    {
		    llTestGPSWord = ( ( pauMinorFrame[iWdIdx] & 0x3FF ) << 10 ) | ( pauMinorFrame[iLSBWdIdx] );
		    }
		else
		    {
		    llTestGPSWord = ( ( pauMinorFrame[iWdIdx] & 0x3FF ) <<  6 ) | ( pauMinorFrame[iLSBWdIdx] >> 4 );
		    }
		}
	    }
	else
	    {
	    llTestGPSWord = pauMinorFrame[psuMeasInfo->uWord];
	    }
	}

    if ( llTestGPSWord > 0 )
	{
	if ( llTestGPSWord != psuPCMInfo->llCurrentGPSWord )
	    {
	    psuPCMInfo->llCurrentGPSWord = llTestGPSWord;
	    psuPCMInfo->ullGPSWordCount++;
	    psuPCMInfo->ullGPSWordStreakCount = 0;
	    }
	else
	    {
	    psuPCMInfo->ullGPSWordCount++;
	    psuPCMInfo->ullGPSWordStreakCount++;
	    }
	return 1;
	}

    return 0;
}

void vSearchMinorFrameFor16BitWord(struct suPCMInfo * psuPCMInfo, struct suMeasurementInfo * psuMeasInfo, uint16_t * pauMinorFrame, int64_t llMinorFrameIdx)
{
    uint16_t    uWdIdx;
    char        acTestWord[5];
    size_t      szKeywordLen;

    acTestWord[4] = '\0';

    szKeywordLen = strlen(psuMeasInfo->szTSSearchWord);

    for ( uWdIdx = 0; uWdIdx < psuPCMInfo->ullSampsPerMinorFrame -1; uWdIdx++ )
	{
	/* acTestWord[0] = (char) ( ( pauMinorFrame[uWdIdx] >> 8 ) & 0xFF ); */
	/* acTestWord[1] = (char) ( pauMinorFrame[uWdIdx]  & 0xFF ); */

	/* acTestWord[2] = (char) ( ( pauMinorFrame[uWdIdx+1] >> 8 ) & 0xFF ); */
	/* acTestWord[3] = (char) ( pauMinorFrame[uWdIdx+1] & 0xFF ); */

	acTestWord[0] = (char) ( ( pauMinorFrame[uWdIdx]        ) & 0xFF );
	acTestWord[1] = (char) ( ( pauMinorFrame[uWdIdx]   >> 8 ) & 0xFF );

	acTestWord[2] = (char) ( ( pauMinorFrame[uWdIdx+1]      ) & 0xFF );
	acTestWord[3] = (char) ( ( pauMinorFrame[uWdIdx+1] >> 8 ) & 0xFF );

	//	printf("the word: %s\n",acTestWord);

	if ( strncmp(acTestWord,psuMeasInfo->szTSSearchWord,szKeywordLen) == 0 )
	    {
		printf("Got a search word! It's like this: %s\n",acTestWord);
		psuMeasInfo->pallWordOffsets[psuMeasInfo->uOffsetBufCount] = uWdIdx;
		psuMeasInfo->uOffsetBufCount++;
	    }
	}
}

int iWriteMeasurementTStamps(struct suPCMInfo * psuPCMInfo, struct suMeasurementInfo * psuMeasInfo, int64_t llMinorFrameIdx,
			     int64_t llWordOffset_MajorFrame, int64_t llWordOffset_MinorFrame, 
			     int64_t llWordOffset_GPS)
{
    int iArgIdx;
    int64_t llBaseOffset;
    int64_t llWordOffset_Measurement;
    double  dTimeOffset_Measurement;

    llBaseOffset = llWordOffset_MajorFrame + llWordOffset_MinorFrame;

    for (iArgIdx = 0; iArgIdx < psuMeasInfo->uOffsetBufCount; iArgIdx++ )
	{
	    llWordOffset_Measurement = (llBaseOffset + psuMeasInfo->pallWordOffsets[iArgIdx]) - llWordOffset_GPS; //rel to current GPS val
	    dTimeOffset_Measurement = llWordOffset_Measurement * psuPCMInfo->dWordPeriod + psuPCMInfo->ullGPSWordCount;
	    fprintf(psuMeasInfo->psuTStampFile,"%.8f\n",dTimeOffset_Measurement);
	}

    //reset the count of the offset buffer
    psuMeasInfo->uOffsetBufCount = 0;

    return 0;
}

int64_t llGetMinorFrameIdx(struct suPCMInfo * psuPCMInfo, uint16_t * pauMinorFrame)
{
    return (pauMinorFrame[psuPCMInfo->uSFIDIdx-1] + 1) & 0b0000111111;  //The TM list counts from 1, not zero
}



uint8_t bBadSFIDIncrement(struct suPCMInfo * psuPCMInfo, int64_t llMinorFrameIdx, int64_t llOldMinorFrameIdx)
{
    return ( ( ( llMinorFrameIdx - llOldMinorFrameIdx ) != 1 ) && ( ( llMinorFrameIdx - llOldMinorFrameIdx ) != 1 - psuPCMInfo->llMinorFramesPerMajorFrame ) );
}

int iPCMFree(struct suPCMInfo * psuPCMInfo)
{
    if ( psuPCMInfo->pauMFCIndices        != NULL ) free( psuPCMInfo->pauMFCIndices );
    if ( psuPCMInfo->paullMajorFrameVals  != NULL ) free( psuPCMInfo->paullMajorFrameVals );
    if ( psuPCMInfo->pauGPSMeasIdx        != NULL ) free( psuPCMInfo->pauGPSMeasIdx );    

    free( psuPCMInfo );

    return (EXIT_SUCCESS);
}

int iMeasurementFree(struct suMeasurementInfo * psuMeasInfo)
{
    
    //    if (psuMeasInfo->uNAsymWRanges > 0)
    //	{
    if ( psuMeasInfo->pauWtemp        != NULL ) free(psuMeasInfo->pauWtemp);
    if ( psuMeasInfo->ppauAsymWRanges != NULL ) free(psuMeasInfo->ppauAsymWRanges);
	//	}
    
    //free asymm frame ranges
	//    if (psuMeasInfo->uNAsymFRanges > 0)
	//	{
    if ( psuMeasInfo->pauFtemp        != NULL ) free(psuMeasInfo->pauFtemp);
    if ( psuMeasInfo->ppauAsymFRanges != NULL ) free(psuMeasInfo->ppauAsymFRanges);
        //      }

    if ( psuMeasInfo->psuOutFile      != NULL )	fclose(psuMeasInfo->psuOutFile);
    if ( psuMeasInfo->psuTStampFile   != NULL ) fclose(psuMeasInfo->psuTStampFile);
    if ( psuMeasInfo->pallWordOffsets != NULL )	free(psuMeasInfo->pallWordOffsets);

    free(psuMeasInfo);

    return (EXIT_SUCCESS);
}

void vPrintSubFrame (uint16_t * pauMajorFrame, int64_t llMinorFrameIdx)
{ 


}

void vPrintPCMInfo (struct suPCMInfo * psuPCMInfo)
{
    printf("\n");		                 
    printf("Subframe ID word index               :   %" PRIu16 "\n",psuPCMInfo->uSFIDIdx);
    printf("TM Link #                            :   %" PRIu16 "\n",psuPCMInfo->uTMLink);
    printf("PCM bit rate                         :   %" PRIu64 "\n",psuPCMInfo->ullBitRate);
    printf("PCM word period (s)                  :   %.8f\n"       ,psuPCMInfo->dWordPeriod);
    printf("N measurements tracked               :   %" PRIu16 "\n",psuPCMInfo->iNMeasurements);
    printf("\n");		                 
    printf("Sample word length in bits           :   %" PRIu64 "\n",psuPCMInfo->ullSampBitLength);
    printf("Number of samples per minor frame    :   %" PRIu64 "\n",psuPCMInfo->ullSampsPerMinorFrame);
    printf("N Minor frames per major frame       :   %" PRIi64 "\n",psuPCMInfo->llMinorFramesPerMajorFrame);
    printf("N bits in minor frame counter        :   %" PRIu16 "\n",psuPCMInfo->uMinorFrameBitShift);
    printf("N major frame counters               :   %" PRIu16 "\n",psuPCMInfo->uNumMFCounters);
    printf("\n");		                 
}

void vPrintMeasurementInfo (struct suMeasurementInfo * psuMeasInfo)
{
    printf("Measurement Name       :   %s\n",psuMeasInfo->szName);
    printf("Abbrev                 :   %s\n",psuMeasInfo->szAbbrev);
    printf("User                   :   %s\n",psuMeasInfo->szUser);
    printf("Word                   :   %" PRIu16 "\n",psuMeasInfo->uWord);
    printf("Word Interval          :   %" PRIu16 "\n",psuMeasInfo->uWdInt);
    //    printf("N Words in Minor Frame :   %" PRIu16 "\n",psuMeasInfo->uSampsPerMinorFrame);
    printf("Minor Frame            :   %" PRIu16 "\n",psuMeasInfo->uMinorFrame);
    printf("Minor Frame Interval   :   %" PRIu16 "\n",psuMeasInfo->uMinorFrInt);
    printf("Samples per second     :   %" PRIu16 "\n",psuMeasInfo->ulSPS);
    printf("\n");
    printf("# Asym word ranges     :   %" PRIu16 "\n",psuMeasInfo->uNAsymWRanges);
    printf("# Asym frame ranges    :   %" PRIu16 "\n",psuMeasInfo->uNAsymFRanges);
    printf("\n");
    printf("TStamp calc enabled    :   %" PRIu8 "\n",psuMeasInfo->bTSCalcEnabled);
}

void vUsage(void)
{
    printf("\n");
    printf("parse_PCM_samples\n");
    printf("Convert a Chapter 10 PCM dump into separate measurement files!             \n");
    printf(" Note: All of the TMs are defined in the respective header files!          \n");
    printf("\n");
    printf("Usage: parse_CAPER_samples [flags]                                         \n");
    printf("                                                                           \n");
    printf("   <filename>   Input/output file names                                    \n");
    printf("                                                                           \n");
    printf("   INPUT FILE PARAMETERS                                                   \n");
    printf("   -L           TM link number (can be 1-4)                        [%i]    \n",DEF_TM_LINK);
    printf("                                                                           \n");
    printf("                                                                           \n");
    printf("   OPTIONAL PARAMETERS                                                     \n");
    printf("   -P           Prefix for output files                           [%s]    \n",DEF_OUTPREFIX);
    printf("   -C           Combine MSB/LSB channels on the fly(TM1 Only!)     [%i]    \n",DEF_COMBINE_TM1);
    printf("   -c           Check integrity of data by following SFID,         [%i]    \n",DEF_DO_CHECK_SFID_INCREMENT);
    printf("                    (no parsed output is produced)                         \n");
    printf("   -A           Assemble unique counter based on major/minor       [%i]    \n",DEF_ASSEMBLE_COUNTER);
    printf("                    frames, and output to file                             \n");
    printf("   -T           Produce timestamps based on GPS pulse data for     [%i]    \n",DEF_CALC_TSTAMPS);
    printf("                    measurements specified in PCM header file              \n");
    printf("   -v           Verbose                                            [%i]    \n",DEF_VERBOSE);
    printf("                                                                           \n");
    printf("   -h           Help (this menu)                                           \n");
}