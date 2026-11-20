#pragma once
/* Wilds sound requests and ported motion trigger frames. */
#define WILDS_AUDIO_RESOURCE_COUNT 144u
#define WILDS_AUDIO_REQUEST_COUNT 148u
static const unsigned wilds_speed_swings[3][4]={{106,107,108,109},{71,72,73,110},{111,112,113,114}};
static const unsigned wilds_release_speed_swings[3][4]={{115,116,117,118},{119,120,121,122},{123,124,125,126}};
static const unsigned wilds_combat_blade_impact_requests[3]={127,128,129};
static const unsigned wilds_swing_requests[]={71,72,73};
static const unsigned wilds_release_impact_requests[]={75,76,77};
static const unsigned wilds_release_blade_impact_requests[]={78,79,80};
#define WILDS_SOUND_SHEATH 74u
#define WILDS_SOUND_DRAW 86u
#define WILDS_SOUND_HANDLING 84u
#define WILDS_SOUND_SPIRIT 64u
#define WILDS_SOUND_COUNTER 59u
#define WILDS_SOUND_EQUIPMENT 87u
static const unsigned wilds_helm_impact_requests[3]={130,131,132};
static const unsigned wilds_iai_impact_requests[3]={93,94,95};
#define WILDS_SOUND_HELM 141u
#define WILDS_SOUND_HELM_JUMP 142u
#define WILDS_SOUND_HELM_CUT 143u
#define WILDS_SOUND_HELM_LAND_GEAR 144u
#define WILDS_SOUND_IAI_CUT 82u
#define WILDS_SOUND_IAI_RECOVERY 60u
#define WILDS_SOUND_IAI_COUNTER 96u
#define WILDS_SOUND_FORESIGHT_COUNTER 97u
#define WILDS_SOUND_THRUST_CHARGE 133u
#define WILDS_SOUND_THRUST_CUT 134u
static const unsigned wilds_body_impact_requests[3]={135,136,137};
static const unsigned wilds_shell_impact_requests[3]={138,139,140};
#define WILDS_SOUND_IAI_WHOOSH 73u
typedef struct {unsigned source;float frame;unsigned request;} WildsAudioCue;
static const WildsAudioCue wilds_audio_cues[]={
{901,10.000000f,60},
{901,15.000000f,72},
{901,30.000000f,54},
{901,33.000000f,73},
{901,84.000000f,85},
{902,13.000000f,73},
{902,33.000000f,71},
{902,84.000000f,72},
{902,91.000000f,61},
{903,14.000000f,64},
{903,30.000000f,71},
{903,42.000000f,61},
{904,19.000000f,71},
{904,23.000000f,145},
{904,33.000000f,72},
{904,74.000000f,73},
{904,78.000000f,146},
{904,111.000000f,147},
{905,15.000000f,63},
{905,39.000000f,87},
{905,41.000000f,54},
{905,44.000000f,73},
{905,48.000000f,87},
{905,65.000000f,60},
{906,10.000000f,60},
{906,19.000000f,58},
{906,27.000000f,71},
{906,41.000000f,72},
{906,107.000000f,85},
{907,10.000000f,60},
{907,20.000000f,72},
{907,42.000000f,60},
{907,43.000000f,64},
{907,44.000000f,73},
{907,51.000000f,54},
{907,59.000000f,71},
{907,129.000000f,85},
{908,12.000000f,87},
{909,15.000000f,87},
{909,69.000000f,87},
{910,9.000000f,64},
{910,26.000000f,72},
{911,14.000000f,64},
{911,26.000000f,73},
{912,25.000000f,71},
{912,60.000000f,72},
{913,8.000000f,72},
{913,36.000000f,73},
{913,73.000000f,62},
{914,8.000000f,55},
{914,9.000000f,56},
{914,10.000000f,73},
{915,18.000000f,87},
{915,66.000000f,87},
{916,23.000000f,59},
{917,10.000000f,59},
{917,25.000000f,73},
{917,129.000000f,74},
{918,7.000000f,59},
{918,15.000000f,71},
{918,34.000000f,58},
{918,40.000000f,72},
{918,56.000000f,83},
{918,144.000000f,85},
{918,150.000000f,60},
{920,10.000000f,60},
{920,13.000000f,72},
{920,30.000000f,54},
{920,31.000000f,73},
{920,84.000000f,85},
{921,10.000000f,60},
{921,25.000000f,72},
{921,30.000000f,54},
{921,84.000000f,85},
{922,28.000000f,72},
{922,40.000000f,54},
{922,42.000000f,73},
{923,20.000000f,73},
{923,49.000000f,64},
{923,56.000000f,71},
{924,20.000000f,71},
{924,53.000000f,64},
{924,56.000000f,72},
{925,22.000000f,72},
{925,39.000000f,60},
{925,52.000000f,64},
{925,60.000000f,73},
};
