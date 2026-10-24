// Fill out your copyright notice in the Description page of Project Settings.
#include "VoxelGrid.h"

#include "Engine/Texture2DArray.h"

void FVoxelGrid::UpdateMaterialData()
{
    // Upload voxel data to texture
    void* TargetTextureData = VoxelDataTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(TargetTextureData, Data.GetData(), Data.Num());
    VoxelDataTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
    VoxelDataTexture->UpdateResource();
}

void FVoxelGrid::CreateMaterialInstance(uint32 VoxelSize, UMaterial* BaseMaterial, UTexture2DArray* VoxelTextures)
{
    // Create material and texture if they don't exist yet
    if (BaseMaterial && VoxelTextures)
    {
        // Create texture to store voxel data
        uint32 TextureHeight = Data.Num() / GridSize;
        VoxelDataTexture = UTexture2D::CreateTransient(GridSize, TextureHeight, PF_R8);
        VoxelDataTexture->CompressionSettings = TextureCompressionSettings::TC_Grayscale;
        VoxelDataTexture->SRGB = 0;
        VoxelDataTexture->Filter = TextureFilter::TF_Nearest;
//#ifndef UE_BUILD_DEBUG
        VoxelDataTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
        VoxelDataTexture->CompressionNone = 1;
//#endif  // !UE_BUILD_DEBUG
// 
        // Upload voxel data to texture
        void* TargetTextureData = VoxelDataTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(TargetTextureData, Data.GetData(), Data.Num());
        VoxelDataTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
        VoxelDataTexture->UpdateResource();

        // Create material instance for chunk mesh
        MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, nullptr);
        MaterialInstance->SetTextureParameterValue(TEXT("VoxelTextures"), VoxelTextures);
        MaterialInstance->SetTextureParameterValue(TEXT("VoxelDataTexture"), VoxelDataTexture);
        MaterialInstance->SetScalarParameterValue(TEXT("VoxelSize"), static_cast<float>(VoxelSize));
        MaterialInstance->SetScalarParameterValue(TEXT("VoxelDataSize"), static_cast<float>(GridSize));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Missing material data for grid, cannot create material instance!"));
    }
}
