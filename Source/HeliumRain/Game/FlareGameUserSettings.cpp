
#include "../Flare.h"
#include "FlareGameUserSettings.h"
#include "FlareGame.h"
#include "../Player/FlarePlayerController.h"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareGameUserSettings::UFlareGameUserSettings(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
}

void UFlareGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	ScreenPercentage = 100;

	UseMotionBlur = true;
	UseCockpit = true;
	PauseGameInMenus = false;

	MusicVolume = 8;
	MasterVolume = 10;
}

void UFlareGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	FLOG("UFlareGameUserSettings::ApplySettings");

	Super::ApplySettings(bCheckForCommandLineOverrides);

	SetScreenPercentage(ScreenPercentage);
}

void UFlareGameUserSettings::SetScreenPercentage(int32 NewScreenPercentage)
{
	FLOGV("UFlareGameUserSettings::SetScreenPercentage %d", NewScreenPercentage);
	ScreenPercentage = NewScreenPercentage;

	auto ScreenPercentageCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	ScreenPercentageCVar->Set(ScreenPercentage, ECVF_SetByGameSetting);
}

void UFlareGameUserSettings::SetMotionBlurEnabled(bool NewUseMotionBlur)
{
	FLOGV("UFlareGameUserSettings::SetMotionBlurEnabled %d", NewUseMotionBlur);
	UseMotionBlur = NewUseMotionBlur;
}
