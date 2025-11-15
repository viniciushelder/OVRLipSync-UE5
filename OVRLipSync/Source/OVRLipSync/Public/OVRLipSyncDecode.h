/*******************************************************************************
 * Filename    :   OVRLipSyncDecode.h
 * Content     :   Runtime Blueprint Nodes for OVRLipSync Decoding
 * Created     :   Nov 15th, 2024
 * Copyright   :   Copyright Facebook Technologies, LLC and its affiliates.
 *                 All rights reserved.
 *
 * Licensed under the Oculus Audio SDK License Version 3.3 (the "License");
 * you may not use the Oculus Audio SDK except in compliance with the License,
 * which is provided at the time of installation or download, or which
 * otherwise accompanies this software in either electronic or hard copy form.

 * You may obtain a copy of the License at
 *
 * https://developer.oculus.com/licenses/audio-3.3/
 *
 * Unless required by applicable law or agreed to in writing, the Oculus Audio SDK
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sound/SoundWave.h"
#include "OVRLipSyncFrame.h"
#include "OVRLipSyncDecode.generated.h"

/**
 * Blueprint function library for runtime OVRLipSync decoding operations
 */
UCLASS()
class OVRLIPSYNC_API UOVRLipSyncDecode : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Converts Base64 encoded WAV data to a USoundWave object at runtime
	 * @param Base64WavData - Base64 encoded WAV file data
	 * @param OutSoundWave - The resulting SoundWave object
	 * @return true if conversion was successful, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "OVRLipSync|Decode")
	static bool Base64ToSoundWave(const FString& Base64WavData, USoundWave*& OutSoundWave);

	/**
	 * Generates a LipSync sequence from a SoundWave at runtime
	 * @param SoundWave - The input SoundWave to process
	 * @param UseOfflineModel - Whether to use the offline model for processing
	 * @param OutSequence - The resulting LipSync frame sequence
	 * @return true if generation was successful, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "OVRLipSync|Decode")
	static bool GenerateLipSyncSequenceRuntime(USoundWave* SoundWave, bool UseOfflineModel, UOVRLipSyncFrameSequence*& OutSequence);

private:
	/**
	 * Helper function to decompress SoundWave and prepare PCM data for runtime processing
	 * @param SoundWave - The SoundWave to decompress
	 * @return true if decompression was successful, false otherwise
	 */
	static bool DecompressSoundWaveRuntime(USoundWave* SoundWave);

	/**
	 * Helper function to parse WAV header and validate format
	 * @param WavData - Raw WAV file data
	 * @param OutSampleRate - Extracted sample rate
	 * @param OutNumChannels - Extracted number of channels
	 * @param OutPCMDataOffset - Offset to PCM data in the WAV file
	 * @param OutPCMDataSize - Size of PCM data
	 * @return true if WAV header is valid, false otherwise
	 */
	static bool ParseWavHeader(const TArray<uint8>& WavData, uint32& OutSampleRate, uint16& OutNumChannels, uint32& OutPCMDataOffset, uint32& OutPCMDataSize);
};
