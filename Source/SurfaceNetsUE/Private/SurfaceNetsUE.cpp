#include "SurfaceNetsUE.h"

DEFINE_LOG_CATEGORY(LogSurfaceNets);

#define LOCTEXT_NAMESPACE "FSurfaceNetsUEModule"

void FSurfaceNetsUEModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogSurfaceNets, Warning, TEXT("SurfaceNetsUE module has been loaded"));
}

void FSurfaceNetsUEModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UE_LOG(LogSurfaceNets, Warning, TEXT("SurfaceNetsUE module has been unloaded"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSurfaceNetsUEModule, SurfaceNetsUE)