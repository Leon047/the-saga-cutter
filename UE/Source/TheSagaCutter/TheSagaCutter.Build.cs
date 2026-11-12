// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TheSagaCutter : ModuleRules
{
	public TheSagaCutter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"GameplayTags", "AIModule", "NavigationSystem", "UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"StateTreeModule", "GameplayStateTreeModule", "SmartObjectsModule", "Slate", "SlateCore"
		});
	}
}
