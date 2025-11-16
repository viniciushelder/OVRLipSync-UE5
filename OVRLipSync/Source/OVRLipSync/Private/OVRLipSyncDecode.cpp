/*******************************************************************************
 * Filename    :   OVRLipSyncDecode.cpp
 * Content     :   Runtime Blueprint Nodes for OVRLipSync Decoding Implementation
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

#include "OVRLipSyncDecode.h"
#include "OVRLipSyncContextWrapper.h"
#include "Misc/Base64.h"
#include "Sound/SoundWave.h"
#include "AudioDevice.h"

DEFINE_LOG_CATEGORY_STATIC(LogOVRLipSyncDecode, Log, All);

namespace
{
	// Compute LipSync sequence frames at 100 times a second rate
	constexpr auto LipSyncSequenceUpdateFrequency = 100;
	constexpr auto LipSyncSequenceDuration = 1.0f / LipSyncSequenceUpdateFrequency;

	// WAV file format constants
	struct FWavHeader
	{
		char RIFF[4];        // "RIFF"
		uint32 ChunkSize;
		char WAVE[4];        // "WAVE"
		char fmt[4];         // "fmt "
		uint32 Subchunk1Size;
		uint16 AudioFormat;  // 1 = PCM
		uint16 NumChannels;
		uint32 SampleRate;
		uint32 ByteRate;
		uint16 BlockAlign;
		uint16 BitsPerSample;
	};

	struct FWavDataHeader
	{
		char data[4];        // "data"
		uint32 DataSize;
	};
}

bool UOVRLipSyncDecode::ParseWavHeader(const TArray<uint8>& WavData, uint32& OutSampleRate, uint16& OutNumChannels, uint32& OutPCMDataOffset, uint32& OutPCMDataSize)
{
	const int32 MinWavSize = static_cast<int32>(sizeof(FWavHeader) + sizeof(FWavDataHeader));
	if (WavData.Num() < MinWavSize)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[ParseWavHeader] WAV data too small: %d bytes"), static_cast<int32>(WavData.Num()));
		return false;
	}

	// Parse WAV header
	const FWavHeader* Header = reinterpret_cast<const FWavHeader*>(WavData.GetData());

	// Validate RIFF header
	if (FMemory::Memcmp(Header->RIFF, "RIFF", 4) != 0)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[ParseWavHeader] Invalid RIFF header"));
		return false;
	}

	// Validate WAVE format
	if (FMemory::Memcmp(Header->WAVE, "WAVE", 4) != 0)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[ParseWavHeader] Invalid WAVE format"));
		return false;
	}

	// Validate fmt chunk
	if (FMemory::Memcmp(Header->fmt, "fmt ", 4) != 0)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[ParseWavHeader] Invalid fmt chunk"));
		return false;
	}

	// Validate PCM format
	if (Header->AudioFormat != 1)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[ParseWavHeader] Unsupported audio format: %u (only PCM is supported)"), Header->AudioFormat);
		return false;
	}

	// Validate bits per sample
	if (Header->BitsPerSample != 16)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[ParseWavHeader] Unsupported bits per sample: %u (only 16-bit is supported)"), Header->BitsPerSample);
		return false;
	}

	// Find data chunk
	uint32 Offset = static_cast<uint32>(sizeof(FWavHeader));
	bool bFoundDataChunk = false;

	while (Offset + static_cast<uint32>(sizeof(FWavDataHeader)) <= static_cast<uint32>(WavData.Num()))
	{
		const FWavDataHeader* DataHeader = reinterpret_cast<const FWavDataHeader*>(WavData.GetData() + Offset);

		if (FMemory::Memcmp(DataHeader->data, "data", 4) == 0)
		{
			OutPCMDataOffset = Offset + static_cast<uint32>(sizeof(FWavDataHeader));
			OutPCMDataSize = DataHeader->DataSize;
			bFoundDataChunk = true;
			break;
		}

		// Skip this chunk and move to next
		Offset += 8; // chunk ID (4) + chunk size field (4)
		if (Offset + 4 <= static_cast<uint32>(WavData.Num()))
		{
			uint32 ChunkSize = *reinterpret_cast<const uint32*>(WavData.GetData() + Offset - 4);
			Offset += ChunkSize;
		}
		else
		{
			break;
		}
	}

	if (!bFoundDataChunk)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[ParseWavHeader] Data chunk not found"));
		return false;
	}

	OutSampleRate = Header->SampleRate;
	OutNumChannels = Header->NumChannels;

	UE_LOG(LogOVRLipSyncDecode, Log, TEXT("[ParseWavHeader] Success - Sample Rate: %u, Channels: %u, PCM Data Size: %u bytes"),
		OutSampleRate, OutNumChannels, OutPCMDataSize);

	return true;
}

bool UOVRLipSyncDecode::Base64ToSoundWave(const FString& Base64WavData, USoundWave*& OutSoundWave)
{
	UE_LOG(LogOVRLipSyncDecode, Log, TEXT("[Base64ToSoundWave] Starting conversion, Base64 string length: %d"), static_cast<int32>(Base64WavData.Len()));

	// Decode Base64
	TArray<uint8> WavData;
	if (!FBase64::Decode(Base64WavData, WavData))
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[Base64ToSoundWave] Failed to decode Base64 data"));
		return false;
	}

	UE_LOG(LogOVRLipSyncDecode, Log, TEXT("[Base64ToSoundWave] Base64 decoded successfully, WAV data size: %d bytes"), static_cast<int32>(WavData.Num()));

	// Parse WAV header
	uint32 SampleRate = 0;
	uint16 NumChannels = 0;
	uint32 PCMDataOffset = 0;
	uint32 PCMDataSize = 0;

	if (!ParseWavHeader(WavData, SampleRate, NumChannels, PCMDataOffset, PCMDataSize))
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[Base64ToSoundWave] Failed to parse WAV header"));
		return false;
	}

	// Validate PCM data
	if (PCMDataOffset + PCMDataSize > static_cast<uint32>(WavData.Num()))
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[Base64ToSoundWave] PCM data exceeds WAV file size"));
		return false;
	}

	// Create SoundWave object
	OutSoundWave = NewObject<USoundWave>();
	if (!OutSoundWave)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[Base64ToSoundWave] Failed to create SoundWave object"));
		return false;
	}

	// Set SoundWave properties
	OutSoundWave->SetSampleRate(SampleRate);
	OutSoundWave->NumChannels = NumChannels;
	OutSoundWave->Duration = static_cast<float>(PCMDataSize) / static_cast<float>(SampleRate * NumChannels * sizeof(int16));
	OutSoundWave->RawPCMDataSize = PCMDataSize;

	// Allocate and copy PCM data
	OutSoundWave->RawPCMData = static_cast<uint8*>(FMemory::Malloc(static_cast<SIZE_T>(PCMDataSize)));
	FMemory::Memcpy(OutSoundWave->RawPCMData, WavData.GetData() + PCMDataOffset, static_cast<SIZE_T>(PCMDataSize));

	UE_LOG(LogOVRLipSyncDecode, Log, TEXT("[Base64ToSoundWave] SoundWave created successfully - Duration: %.2f seconds"), OutSoundWave->Duration);

	return true;
}

bool UOVRLipSyncDecode::DecompressSoundWaveRuntime(USoundWave* SoundWave)
{
	if (!SoundWave)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[DecompressSoundWaveRuntime] SoundWave is null"));
		return false;
	}

	// Already have PCM data
	if (SoundWave->RawPCMData && SoundWave->RawPCMDataSize > 0)
	{
		UE_LOG(LogOVRLipSyncDecode, Log, TEXT("[DecompressSoundWaveRuntime] SoundWave already has RawPCM data"));
		return true;
	}

	UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[DecompressSoundWaveRuntime] SoundWave has no RawPCM data - use Base64ToSoundWave to create SoundWave with PCM data"));
	return false;
}

bool UOVRLipSyncDecode::GenerateLipSyncSequenceRuntime(USoundWave* SoundWave, bool UseOfflineModel, UOVRLipSyncFrameSequence*& OutSequence)
{
	UE_LOG(LogOVRLipSyncDecode, Log, TEXT("[GenerateLipSyncSequenceRuntime] Starting LipSync sequence generation"));

	if (!SoundWave)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[GenerateLipSyncSequenceRuntime] SoundWave is null"));
		return false;
	}

	// Validate channel count
	if (SoundWave->NumChannels > 2)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[GenerateLipSyncSequenceRuntime] Only mono and stereo streams are supported, got %d channels"), static_cast<int32>(SoundWave->NumChannels));
		return false;
	}

	// Attempt to decompress/get PCM data
	if (!DecompressSoundWaveRuntime(SoundWave))
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[GenerateLipSyncSequenceRuntime] Failed to get PCM data from SoundWave"));
		return false;
	}

	// Defensive checks
	if (SoundWave->RawPCMData == nullptr || SoundWave->RawPCMDataSize <= 0)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[GenerateLipSyncSequenceRuntime] SoundWave has no RawPCMData"));
		return false;
	}

	UE_LOG(LogOVRLipSyncDecode, Log, TEXT("[GenerateLipSyncSequenceRuntime] SoundWave validated - Channels: %d, Sample Rate: %d, PCM Size: %u"),
		static_cast<int32>(SoundWave->NumChannels), static_cast<int32>(SoundWave->GetSampleRateForCurrentPlatform()), SoundWave->RawPCMDataSize);

	// Create LipSync sequence
	OutSequence = NewObject<UOVRLipSyncFrameSequence>();
	if (!OutSequence)
	{
		UE_LOG(LogOVRLipSyncDecode, Error, TEXT("[GenerateLipSyncSequenceRuntime] Failed to create LipSync sequence object"));
		return false;
	}

	int32 NumChannels = static_cast<int32>(SoundWave->NumChannels);
	int32 SampleRate = static_cast<int32>(SoundWave->GetSampleRateForCurrentPlatform());
	int32 PCMDataSize = static_cast<int32>(SoundWave->RawPCMDataSize / sizeof(int16));
	int16* PCMData = reinterpret_cast<int16*>(SoundWave->RawPCMData);
	int32 ChunkSizeSamples = static_cast<int32>(static_cast<float>(SampleRate) * LipSyncSequenceDuration);
	int32 ChunkSize = NumChannels * ChunkSizeSamples;

	// Setup model path for offline model if requested
	FString ModelPath;
	if (UseOfflineModel)
	{
		ModelPath = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("OVRLipSync"), TEXT("OfflineModel"), TEXT("ovrlipsync_offline_model.pb"));
		if (!FPaths::FileExists(ModelPath))
		{
			UE_LOG(LogOVRLipSyncDecode, Warning, TEXT("[GenerateLipSyncSequenceRuntime] Offline model not found at: %s"), *ModelPath);
			ModelPath = FString();
		}
		else
		{
			UE_LOG(LogOVRLipSyncDecode, Log, TEXT("[GenerateLipSyncSequenceRuntime] Using offline model: %s"), *ModelPath);
		}
	}

	// Create LipSync context
	UOVRLipSyncContextWrapper Context(ovrLipSyncContextProvider_Enhanced, SampleRate, 4096, ModelPath);

	float LaughterScore = 0.0f;
	int32 FrameDelayInMs = 0;
	TArray<float> Visemes;

	// Initialize with zero samples to get frame delay
	TArray<int16> Samples;
	Samples.SetNumZeroed(ChunkSize);
	Context.ProcessFrame(Samples.GetData(), ChunkSizeSamples, Visemes, LaughterScore, FrameDelayInMs, NumChannels > 1);

	int32 FrameOffset = static_cast<int32>(FrameDelayInMs * SampleRate / 1000 * NumChannels);

	UE_LOG(LogOVRLipSyncDecode, Log, TEXT("[GenerateLipSyncSequenceRuntime] Processing frames - Total samples: %d, Frame offset: %d, Chunk size: %d"),
		PCMDataSize, FrameOffset, ChunkSize);

	int32 FrameCount = 0;
	for (int32 Offs = 0; Offs < PCMDataSize + FrameOffset; Offs += ChunkSize)
	{
		int32 RemainingSamples = PCMDataSize - Offs;
		if (RemainingSamples >= ChunkSize)
		{
			Context.ProcessFrame(PCMData + Offs, ChunkSizeSamples, Visemes, LaughterScore, FrameDelayInMs, NumChannels > 1);
		}
		else
		{
			if (RemainingSamples > 0)
			{
				FMemory::Memcpy(Samples.GetData(), PCMData + Offs, static_cast<SIZE_T>(sizeof(int16)) * RemainingSamples);
				FMemory::Memset(Samples.GetData() + RemainingSamples, 0, static_cast<SIZE_T>(sizeof(int16)) * (ChunkSize - RemainingSamples));
			}
			else
			{
				FMemory::Memset(Samples.GetData(), 0, static_cast<SIZE_T>(sizeof(int16)) * ChunkSize);
			}
			Context.ProcessFrame(Samples.GetData(), ChunkSizeSamples, Visemes, LaughterScore, FrameDelayInMs, NumChannels > 1);
		}

		if (Offs >= FrameOffset)
		{
			OutSequence->Add(Visemes, LaughterScore);
			FrameCount++;
		}
	}

	UE_LOG(LogOVRLipSyncDecode, Log, TEXT("[GenerateLipSyncSequenceRuntime] LipSync sequence generated successfully - Total frames: %d"), FrameCount);

	return true;
}
