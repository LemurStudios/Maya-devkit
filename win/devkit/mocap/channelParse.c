/*
//-
// ==========================================================================
// Copyright 1995,2006,2008 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk
// license agreement provided at the time of installation or download,
// or which otherwise accompanies this software in either electronic
// or hard copy form.
// ==========================================================================
//+
*/

#include <stdio.h>
#include <math.h>
#ifndef _WIN32
#include <strings.h>
#else
#define strcasecmp _stricmp
#define	M_PI		3.14159265358979323846	/* pi */
#endif
#include "channelParse.h"

#ifndef lengthof
#define lengthof(array)	(sizeof(array) / sizeof(array[0]))
#endif /* lengthof */

#define kBadUnitName			"Unknown unit name"
#define kBadRotOrderName		"Unknown rotation order"
#define kExpectingThreeValuesIn	"Expected three values for keyword"
#define kExpectingNumberFound	"Expecting a floating point number, found"
#define kBadKeyword				"Unrecogized channel command"
#define kMissingChannelName 	"Expected channel name, found nothing"
#define kBadAlloc 				"Could not allocate memory for operation"
#define kUnknownDimSize			"SetData"
#define kGreaterThan7			"data size > 7"

static channelParseUnits channelUnitsList[] = {

	"DEG",	"DEGREE",     kRotation, (M_PI / 180.),
	"RAD",	"RADIAN",     kRotation, 1.,
	"MM",	"MILLIMETER", kPosition, 10.,
	"CM",	"CENTIMETER", kPosition, 1.,
	"M",	"METER",      kPosition, 1./100.,
	"IN",	"INCH",       kPosition, 2.54,
	"FT",   "FOOT",       kPosition, 12. * 2.54
};

static channelParseInfo channelParseList[] = {
	/*
	  Short   Long         Usage                Dimension
	  Name    Name          (see mocapserver.h)     Extra size 
                                                       Axis offset
													           Type offset */
	"UNK",  "UNKNOWN",    CAP_USAGE_UNKNOWN,     1, 0, kInval, kInvalid,
	"POS",  "POSITION",   CAP_USAGE_POSITION,    3, 0, kXAxis, kPosition,
	"ROT",  "ROTATION",   CAP_USAGE_ORIENTATION, 3, 1, kXAxis, kRotation,
	"QUAT", "QUATERNION", CAP_USAGE_ORIENTATION, 4, 0, kInval, kInvalid,
	"PR",   "POS_ROT",    CAP_USAGE_POS_ORIENT,  6, 1, kInval, kInvalid,
	"PQ",   "POS_QUAT",   CAP_USAGE_POS_ORIENT,  7, 0, kInval, kInvalid,
	"X",    "XPOS",       CAP_USAGE_XPOS,        1, 0, kXAxis, kPosition,
	"Y",    "YPOS",       CAP_USAGE_YPOS,        1, 0, kYAxis, kPosition,
	"Z",    "ZPOS",       CAP_USAGE_ZPOS,        1, 0, kZAxis, kPosition,
	"RX",   "XROT",       CAP_USAGE_XROT,        1, 0, kXAxis, kRotation,
	"RY",   "YROT",       CAP_USAGE_YROT,        1, 0, kYAxis, kRotation,
	"RZ",   "ZROT",       CAP_USAGE_ZROT,        1, 0, kZAxis, kRotation,
	"SX",   "XSCALE",     CAP_USAGE_XSCALE,      1, 0, kXAxis, kScale,
	"SY",   "YSCALE",     CAP_USAGE_YSCALE,      1, 0, kYAxis, kScale,
	"SZ",   "ZSCALE",     CAP_USAGE_ZSCALE,      1, 0, kZAxis, kScale,
	"S",    "SCALE",      CAP_USAGE_SCALE,       1, 0, kXAxis, kScale,
	"SXYZ", "SCALE3",     CAP_USAGE_SCALE,       3, 0, kXAxis, kScale,

	"RO",	"ROTORDER",   CAP_USAGE_NONE,	1, kRotOrder, kInval,  kInvalid,
	"OP",	"OFFSETPOS",  CAP_USAGE_NONE,	3, kFactors,  kOffset, kPosition,
	"OR",	"OFFSETROT",  CAP_USAGE_NONE,	3, kFactors,  kOffset, kRotation,
	"OS",	"OFFSETSCALE",CAP_USAGE_NONE,	3, kFactors,  kOffset, kScale,
	"SP",	"SCALEPOS",   CAP_USAGE_NONE,	3, kFactors,  kMult,   kPosition,
	"SR",	"SCALEROT",   CAP_USAGE_NONE,	3, kFactors,  kMult,   kRotation,
	"SS",	"SCALESCALE", CAP_USAGE_NONE,	3, kFactors,  kMult,   kScale,
	"UN",	"UNIT",       CAP_USAGE_NONE,	1, kUnit,     kInval,  kInvalid,

	"BEGIN_CHANNEL_INFO","BEGIN_CHANNEL_INFO", 
	                      CAP_USAGE_NONE,   0, kStartParse,  0,kInvalid,
	"END_CHANNEL_INFO",  "END_CHANNEL_INFO",   
	                      CAP_USAGE_NONE,   0, kEndParse,    0,kInvalid
};



static channelParseRotOrder channelRotOrderList[] = {
	"XY", "XYZ",   CAP_ROT_XYZ,
	"XZ", "XZY",   CAP_ROT_XZY,
	"YX", "YXZ",   CAP_ROT_YXZ,
	"YZ", "YZX",   CAP_ROT_YZX,
	"ZX", "ZXY",   CAP_ROT_ZXY,
	"ZY", "ZYX",   CAP_ROT_ZYX,

	"KXY", "KXYZ", CAP_ROT_XYZ_KIN,
	"KXZ", "KXZY", CAP_ROT_XZY_KIN,
	"KYX", "KYXZ", CAP_ROT_YXZ_KIN,
	"KYZ", "KYZX", CAP_ROT_YZX_KIN,
	"KZX", "KZXY", CAP_ROT_ZXY_KIN,
	"KZY", "KZYX", CAP_ROT_ZYX_KIN,

	"MXY", "MXYZ", CAP_ROT_XYZ_MAYA,
	"MXZ", "MXZY", CAP_ROT_XZY_MAYA,
	"MYX", "MYXZ", CAP_ROT_YXZ_MAYA,
	"MYZ", "MYZX", CAP_ROT_YZX_MAYA,
	"MZX", "MZXY", CAP_ROT_ZXY_MAYA,
	"MZY", "MZYX", CAP_ROT_ZYX_MAYA,

};

/* ZZZ_LATER: access functions to change these... */

static FILE *channelInfoErrFile = 0;
static const char *channelInfoErrPrefix = "channelInfoError";
static const char *channelParseSep = "\t \n";
static void channelInfoErr(const char *m1, const char *m2) {

			if (channelInfoErrFile == 0)
				channelInfoErrFile = stderr;

			fprintf(channelInfoErrFile,"%s: %s: %s\n",
					channelInfoErrPrefix, m1, m2);
			fflush(channelInfoErrFile);
}



channelParseInfo *channelInfoFindParseMatch(char *token)
/*
 * Description:
 * search the keyword list for what kind of line this is and return it.
 */
{
	int i;
	channelParseInfo *cpi = channelParseList;
	channelParseInfo *match = NULL;
	for (i=0; i < lengthof(channelParseList); i++) {
		if ( ( 0 == strcasecmp(token, cpi[i].shortName) ) ||
			 ( 0 == strcasecmp(token, cpi[i].longName ) )  ) {
			match = &cpi[i];
			break;
		}
	}
	return match;
}

static channelInfo *channelInfoNew(	channelParseInfo *pi,
									channelInfo 	 *tail,
									char			 *name,
									float            factors[2][3][3],
									float			 unitScale[3],
									CapRotationOrder rotOrder) 
/* Description:
 *	based on the parse info and the current state create a cap channel
 */
{
	int i;
	channelInfo *chan = (channelInfo *)malloc(sizeof(channelInfo));
	int size = pi->p1.dim + pi->p2.extra;

	if ( NULL == chan )
		return chan;

	chan->chan = CapCreateChannel(name, pi->use, size);
	chan->info   = pi;

	if ( NULL != tail ) {
		tail->next = chan;
		chan->startingCol = tail->startingCol + tail->info->p1.dim;
	} else
		chan->startingCol = 0;

	/* Copy all of the factors multing on the units */
	memcpy(chan->factors,factors,sizeof(float)*2*3*3);
	for (i=0 ; i < 3; i++ ) {
		chan->factors[kMult][kRotation][i] *= unitScale[kRotation];
		chan->factors[kMult][kPosition][i] *= unitScale[kPosition];
		chan->factors[kMult][kScale][i]    *= unitScale[kScale];
	}


	chan->rotOrder = rotOrder;
	chan->data     = (float *)malloc(sizeof(float) * pi->p1.dim);
	chan->name     = (char *)malloc(strlen(name)+1);
	strcpy(chan->name, name);
	chan->next = NULL;

	return chan;
}
			
int channelInfoParseUnits( float unitScale[3]) 
/*
 * Description:
 *	read the next token from the input and match it against the
 *  list of valid unit names
 */
{
	int i;
	channelParseUnits *cpu = channelUnitsList;
	channelParseUnits *match = NULL;

	char *token = strtok(NULL,channelParseSep);
	for (i=0; i < lengthof(channelUnitsList); i++) {
		if ( ( 0 == strcasecmp(token, cpu[i].shortName) ) ||
			 ( 0 == strcasecmp(token, cpu[i].longName ) )  ) {
			match = &cpu[i];
			break;
		}
	}

	if ( (NULL == match) || (kInvalid == match->parm) ) {
		channelInfoErr(kBadUnitName,token);
		return 0;
	}
	
	unitScale[match->parm] = match->value;

	return 1;
}
		
static int channelInfoParseOrder(CapRotationOrder *rotOrder)  
/*
 * Description:
 *	read the next token from the input and match it against the
 *  list of valid rotational order names
 */
{
	int i;
	channelParseRotOrder *cpr= channelRotOrderList;
	channelParseRotOrder *match = NULL;

	char *token = strtok(NULL,channelParseSep);
	for (i=0; i < lengthof(channelRotOrderList); i++) {
		if ( ( 0 == strcasecmp(token, cpr[i].shortName) ) ||
			 ( 0 == strcasecmp(token, cpr[i].longName ) )  ) {
			match = &cpr[i];
			break;
		}
	}

	if ( NULL == match ) {
		channelInfoErr(kBadRotOrderName,token);
		return 0;
	}

	*rotOrder = match->value;

	return 1;
}

static int	channelInfoParseScaleOffset(channelParseInfo *pi, 
										float            scaleOffset[2][3][3])
/*
 * Description:
 *	read the next 3 tokens from the input and assign them at to the entry
 *  in the scaleOffset array
 */
{
	char *xToken = strtok(NULL,channelParseSep);
	char *yToken = strtok(NULL,channelParseSep);
	char *zToken = strtok(NULL,channelParseSep);
	float *target = scaleOffset[pi->p3.factorType][pi->typeOffset];

	float x, y, z;

	

	if ( ( NULL == xToken ) ||( NULL == xToken ) ||( NULL == xToken ) ) {
		channelInfoErr(kExpectingThreeValuesIn,pi->longName);
		return 0;
	}

	if ( 1 != sscanf( xToken, "%g", &x ) ) {
		channelInfoErr(kExpectingNumberFound,xToken);
		return 0;
	}
		
	if ( 1 != sscanf( yToken, "%g", &y ) ) {
		channelInfoErr(kExpectingNumberFound,yToken);
		return 0;
	}

	if ( 1 != sscanf( zToken, "%g", &z ) ) {
		channelInfoErr(kExpectingNumberFound,zToken);
		return 0;
	}

	target[0] = x;
	target[1] = y;
	target[2] = z;

	return 1;
}

channelInfo *channelInfoCreate(FILE *configFile, 
							   int lookForBegin,
							   channelInfo *head) {
#define LINE_SIZE 4096
	char line[LINE_SIZE];
	char *lp;
	char *token;
	int i;
    float scaleOffset[2][3][3];
	channelParseInfo *match;
	channelInfo *tail, *next;
	char *newName;

	float unitScale[3];
	float posUnitScale = 1.0;
	CapRotationOrder rotOrder = CAP_ROT_XYZ;
	char parsing = !lookForBegin;

	/* a valid file pointer is required */
	if ( NULL == configFile )
		return head;

	/* but head can be NULL or an extant list. */
	tail = next = head;	
	while ( NULL != next ) {
		tail = next;
		next = tail->next;
	}

	/* set default scale and offset */
	for ( i=0; i<3; i++ ) {
		scaleOffset[kOffset][kPosition][i] = 
			scaleOffset[kOffset][kRotation][i] = 
			scaleOffset[kOffset][kScale   ][i] = 0.;

		scaleOffset[kMult  ][kPosition][i] = 
			scaleOffset[kMult  ][kRotation][i] = 
			scaleOffset[kMult  ][kScale   ][i] = 1.;
	}

 	unitScale[kPosition] = 
		unitScale[kRotation] = 
		unitScale[kScale   ] = 1.;
   
	while(1) /* go until a break */ {
		/* Get and tokenize the line */
		lp = fgets (line, LINE_SIZE, configFile);
		if ( NULL == lp )
			break;
		
		token = strtok(lp, channelParseSep);

		if ( NULL == token )
			continue;

		/* Look for a known keyword */
		match = channelInfoFindParseMatch(token);
		
		/* Start-up logic... */
		if ( !parsing ) {
			if ( ( NULL != match )
				 && ( match->use == CAP_USAGE_NONE ) 
				 && ( match->p2.opCode == kStartParse ) ) {
				parsing = 1;	
			}	
			continue;
		}

		/* we're supposedly parsing our own stuff, anything we don't
		 * recognize is BAD.
		 */
		if ( NULL == match ) {
			channelInfoErr(kBadKeyword,token);
			break;
		}


		/* Parsing is true... */
		if ( match->use == CAP_USAGE_NONE ) {
			/* set a "state" parameter. */
			char seeYa = 0;
			
			switch (match->p2.opCode) {
			case kEndParse  :
				seeYa = 1;
				break;
			case kUnit      :
				seeYa = 0 == channelInfoParseUnits(unitScale);
				break;
			case kRotOrder  :
				seeYa = 0 == channelInfoParseOrder(&rotOrder);
				break;
			case kFactors :
				seeYa = 0 == channelInfoParseScaleOffset(match,scaleOffset);
			}

			if ( seeYa )
				break;

			continue;
		}

		/* Add a channel given the current state information */
			
		if ( NULL == ( newName = strtok(NULL,channelParseSep) ) ) {
			channelInfoErr(kMissingChannelName,token);
			break;
		}

		tail = channelInfoNew(match, tail, newName, scaleOffset, 
							  unitScale, rotOrder);

		if ( NULL == tail ) {
			channelInfoErr(kBadAlloc,token);
			break;
		}

		/* If this is the first one initialize the "head of list" */
		if ( NULL == head )
			head = tail;
		
	}

	/* we hand back a pointer to the beginning of the channel list */
	return head;
}

void channelInfoCookData(float *cooked, channelInfo *chan,
						 int start,int dim,int typ, int off) {
	int i;
	for (i=start; i <(start + dim); i++) {
		float dOff = chan->factors[kOffset][typ][i-start+off];
		float dMul = chan->factors[kMult  ][typ][i-start+off];
		cooked[i] = ( (chan->data[i]) + dOff ) * dMul;
	}
}

void channelInfoRot2Quat(float *cooked, CapRotationOrder order )
/* Description:
 *	convert the cooked rotations into quaternions
 */
{
	float x = cooked[0];
	float y = cooked[1];
	float z = cooked[2];
	CapEuler2Quat(order, x, y, z, cooked );
#if 1 /* TEST CODE */
	CapQuat2Euler(order, cooked,
		   &x, &y, &z);
#endif

}
	
void channelInfoSetData(channelInfo *chan, int follow, float *rawData)
/* Description:
 *	Using the rawData values from "rawData" compute and set the data for
 *  chan and links (if follow).
 *
 *  If "rawData" is null the current "
 */
{
	float cooked[7];
	int i;
	while ( NULL != chan ) {
		const int dim = chan->info->p1.dim;
		const int typ = chan->info->typeOffset;
		const int off = chan->info->p3.axisOffset;

		/* if present, copy the raw data into the channelInfo */
		if ( NULL != rawData ) {
			for ( i=0; i < dim; i++ )
				chan->data[i] = *(rawData++);
		}

		/* take the data, offset scale and convert as useful and set it
		 * in the CapChannel as the current value
		 */
		if ( dim < 4 ) {
			/*
			 * this is simple data... (mostly)
			 */
			channelInfoCookData(cooked, chan, 0, dim, typ, off);
			if ( 1 == chan->info->p2.extra ) 
				channelInfoRot2Quat(cooked, chan->rotOrder);
		} else if ( dim == 4 ) {
			for (i=0; i <dim; i++)
				cooked[i] = chan->data[i];
		} else if ( dim == 6 ) {
			/* position & rotation */
			channelInfoCookData(cooked, chan, 0, 3, kPosition, 0);

			channelInfoCookData(cooked, chan, 3, 3, kRotation, 0);
			channelInfoRot2Quat(&cooked[3], chan->rotOrder);
		} else if ( dim == 7 ) {
			/* position & quaternion */
			channelInfoCookData(cooked, chan, 0, 3, kPosition, 0);
			for (i=3; i <dim; i++)
				cooked[i] = chan->data[i];
		} else {
			channelInfoErr(kUnknownDimSize,kGreaterThan7);
			break;
		}

		/* Now set the data in the Cap Channel. */
		CapSetData(chan->chan,cooked);

		/*
		 * if we are not following the linked list, we're done
		 */
		if ( !follow )
			break;

		chan = chan->next;

	}
}
