#DESCRIPTION OF TM link to be read by parse_PCM_samples (v0.3)
#
#NOTE the following:
#  -->A line beginning with '#' is treated as a comment
#
#  -->A line beginning with ';' marks the end of an array. 
#     If ';' is the ONLY value below an array keyword (e.g., MEAS_LSB_WORD), it is special shorthand to indicate
#        that all corresponding array values should be zero.
#
#  -->A line beginning with any of the following is read into the program:
#
#	Values
#	======
#	NAME
#       TM_NUM                                   //Number of the TM link containing this PCM channel
#	N_MEASUREMENTS                           //Number of measurements in this link
#	SFID_IDX				 //Location of subframe ID within minor frame, counting from 1
#	BPS					 //Transfer rate of this link, in bits per second
#						                                                                                       
#	SAMPLE_BITLENGTH                         //Length of each sample in bits (remember, 8 bits per byte)
#	N_SAMPLES_PER_MINOR			 //Number of samples in each minor frame
#	N_MINOR_PER_MAJOR			 //Number of minor frames per major frame
#	N_MAJFRAMECOUNTERS			 //Number of major frame counters in this link
#	N_MINFRAME_BITPOS			 //Number of bits required for minor frame counter (e.g., up to 4 minor frames requires
#					         //  two bit positions to count 0, 1, 2, 3). This is used to form a unique counter from
#	N_GPS_WORDS				 //Number of GPS words defined in the PCM stream. (Max is two!)
#						 
#	Arrays
#	======
#	MEAS_NAME
#	MEAS_ABBREV
#	MEAS_USER
#	MEAS_SAMPLE_LOC_IN_MINFRAME
#	MEAS_SAMPLE_INTERVAL_IN_MINFRAME
#	MEAS_SAMPLE_FRAME
#	MEAS_FRAME_INTERVAL
#	MEAS_SAMPLE_RATE
#	MEAS_N_ASYM_WORD_RANGES
#	MEAS_ASYM_WORD_RANGES
#	MEAS_N_ASYM_FRAME_RANGES
#	MEAS_ASYM_FRAME_RANGES
#	MEAS_LSB_WORD
#	MEAS_TSTAMP_CALC_TYPE
#	MEAS_TSTAMP_SEARCH_WORD
#	MEAS_INTERN_SAMP_RATE
#	MAJFRAME_INDEX
#	GPS_INDEX
#
#******SPECIAL VALUES******
#
#MEAS_TSTAMP_SEARCH_WORD: 0x00 corresponds to NO_TS_SEARCH, or no search word
#
#MEAS_LSB_WORD          : '499' corresponds to TM_SKIP_LSB, so that if channels are being combined, this channel is not outputted
#			    e.g., For ELF-AHI_MSB and ELF-AHI_LSB, the ELF-AHI_LSB entry for MEAS_LSB_WORD is '499' to indicate that
#                           we don't want to output ELF-AHI_LSB as its own channel.
#                       : '498' corresponds to TM_NO_LSB, to indicate that this channel has no associated LSB word.
#
#
#******Note on timestamp calculation (MEAS_TSTAMP_CALC_TYPE)******
#
#CalcType = 1: TStamps calculated relative to GPS word, samples output to binary
#CalcType = 2: TStamps calculated relative to GPS word, samples and TStamps output to ASCII
#Calctype = 3: TStamps calculated rel. to TSSW, using measurement's word period.
#Calctype = 4: TStamps for searchword are outputted along with the TSSW sample number where the searchword was found
##################################################################################
