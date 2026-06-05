
#include <TargetConditionals.h>
#if TARGET_OS_IOS == 1
  #import <UIKit/UIKit.h>
#else
  #import <Cocoa/Cocoa.h>
#endif

#define IPLUG_AUVIEWCONTROLLER IPlugAUViewController_vImpulse
#define IPLUG_AUAUDIOUNIT IPlugAUAudioUnit_vImpulse
#import <ImpulseAU/ImpulseAU.h>

//! Project version number for ImpulseAU.
FOUNDATION_EXPORT double ImpulseAUVersionNumber;

//! Project version string for ImpulseAU.
FOUNDATION_EXPORT const unsigned char ImpulseAUVersionString[];

@class IPlugAUViewController_vImpulse;
