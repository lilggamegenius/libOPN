// Stream.c: C Source File for Sound Output
//

#include <stdio.h>
#include <stdbool.h>
#include <windows.h>

#include "Stream.h"

static DWORD WINAPI WaveOutThread(void *Arg);

static void BufCheck();

uint16_t AUDIOBUFFERU = 10;        // used AudioBuffers

WAVEFORMATEX WaveFmt;
extern uint32_t SampleRate;
bool PauseThread;

static HWAVEOUT hWaveOut;
static WAVEHDR WaveHdrOut[AUDIOBUFFERS];
static HANDLE hWaveOutThread;

static bool WaveOutOpen;
uint32_t BUFFERSIZE;    // Buffer Size in Bytes
uint32_t SMPL_P_BUFFER;
static char BufferOut[AUDIOBUFFERS][BUFSIZE_MAX];
static volatile bool CloseThread;

bool SoundLog;
static FILE *hFile;
uint32_t SndLogLen;

uint32_t BlocksSent;
uint32_t BlocksPlayed;

char SoundLogFile[MAX_PATH];

uint8_t SaveFile(uint32_t FileLen, void *TempData){
	long int TempVal[0x2];

	if(TempData == NULL){
		switch(FileLen){
			case 0x00000000: SndLogLen = 0;
				hFile = fopen(SoundLogFile, "wb");
				if(hFile == NULL){
					// Save Error
					return 0xFF;
				}
				fseek(hFile, 0x00000000, SEEK_SET);
				TempVal[0x0] = 0x46464952;
				TempVal[0x1] = 0x00000000;
				fwrite(&TempVal[0x0], 4, 2, hFile);
				TempVal[0x0] = 0x45564157;
				TempVal[0x1] = 0x20746D66;
				fwrite(&TempVal[0x0], 4, 2, hFile);
				TempVal[0x0] = 0x00000010;
				fwrite(&TempVal[0x0], 4, 1, hFile);
				fwrite(&WaveFmt, TempVal[0x0], 1, hFile);
				TempVal[0x0] = 0x61746164;
				TempVal[0x1] = 0x00000000;
				fwrite(&TempVal[0x0], 4, 2, hFile);
				break;
			case 0xFFFFFFFF: TempVal[0x1] = SndLogLen * SAMPLESIZE;
				TempVal[0x0] = TempVal[0x1] + 0x00000024;
				fseek(hFile, 0x0004, SEEK_SET);
				fwrite(&TempVal[0x0], 4, 1, hFile);
				fseek(hFile, 0x0028, SEEK_SET);
				fwrite(&TempVal[0x1], 4, 1, hFile);
				fclose(hFile);
				hFile = NULL;
				break;
			default: break;
		}
	}else{
		SndLogLen += fwrite(TempData, SAMPLESIZE, FileLen, hFile);
	}

	return 0x00;
}

uint8_t SoundLogging(uint8_t Mode){
	uint8_t RetVal;

	RetVal = (uint8_t) SoundLog;
	switch(Mode){
		case 0x00: SoundLog = false;
			break;
		case 0x01: SoundLog = true;
			if(WaveOutOpen && hFile == NULL){
				SaveFile(0x00000000, NULL);
			}
			break;
		case 0xFF: break;
		default: RetVal = 0xA0;
			break;
	}

	return RetVal;
}

uint8_t StartStream(uint8_t DeviceID){
	uint32_t RetVal;
	uint16_t Cnt;
	HANDLE WaveOutThreadHandle;
	DWORD WaveOutThreadID;

	if(WaveOutOpen){
		return 0xD0;
	}   // Thread is already active

	// Init Audio
	WaveFmt.wFormatTag = WAVE_FORMAT_PCM;
	WaveFmt.nChannels = 2;
	WaveFmt.nSamplesPerSec = SampleRate;
	WaveFmt.wBitsPerSample = 16;
	WaveFmt.nBlockAlign = WaveFmt.wBitsPerSample * WaveFmt.nChannels / 8;
	WaveFmt.nAvgBytesPerSec = WaveFmt.nSamplesPerSec * WaveFmt.nBlockAlign;
	WaveFmt.cbSize = 0;
	if(DeviceID == 0xFF){
		return 0x00;
	}

	BUFFERSIZE = SampleRate / 100 * SAMPLESIZE;
	if(BUFFERSIZE > BUFSIZE_MAX){
		BUFFERSIZE = BUFSIZE_MAX;
	}
	SMPL_P_BUFFER = BUFFERSIZE / SAMPLESIZE;
	if(AUDIOBUFFERU > AUDIOBUFFERS){
		AUDIOBUFFERU = AUDIOBUFFERS;
	}

	PauseThread = true;
	CloseThread = false;

	WaveOutThreadHandle = CreateThread(NULL, 0x00, &WaveOutThread, NULL, 0x00,
	                                   &WaveOutThreadID);
	if(WaveOutThreadHandle == NULL){
		return 0xC8;    // CreateThread failed
	}
	CloseHandle(WaveOutThreadHandle);

	RetVal = waveOutOpen(&hWaveOut, ((UINT) DeviceID - 1), &WaveFmt, 0x00, 0x00, CALLBACK_NULL);
	if(RetVal != MMSYSERR_NOERROR){
		CloseThread = true;
		return 0xC0;    // waveOutOpen failed
	}
	WaveOutOpen = true;

	for(Cnt = 0x00; Cnt < AUDIOBUFFERU; Cnt++){
		WaveHdrOut[Cnt].lpData = BufferOut[Cnt];
		WaveHdrOut[Cnt].dwBufferLength = BUFFERSIZE;
		WaveHdrOut[Cnt].dwBytesRecorded = 0x00;
		WaveHdrOut[Cnt].dwUser = 0x00;
		WaveHdrOut[Cnt].dwFlags = 0x00;
		WaveHdrOut[Cnt].dwLoops = 0x00;
		WaveHdrOut[Cnt].lpNext = NULL;
		WaveHdrOut[Cnt].reserved = 0x00;

		waveOutPrepareHeader(hWaveOut, &WaveHdrOut[Cnt], sizeof(WAVEHDR));
		WaveHdrOut[Cnt].dwFlags |= WHDR_DONE;
	}

	if(SoundLog){
		SaveFile(0x00000000, NULL);
	}

	PauseThread = false;

	return 0x00;
}

uint8_t StopStream(bool SkipWOClose){
	uint32_t RetVal;
	uint16_t Cnt;

	if(!WaveOutOpen){
		return 0xD8;
	}    // Thread is not active

	CloseThread = true;
	for(Cnt = 0; Cnt < 100; Cnt++){
		Sleep(1);
		if(hWaveOutThread == NULL){
			break;
		}
	}
	if(hFile != NULL){
		SaveFile(0xFFFFFFFF, NULL);
	}
	WaveOutOpen = false;

	waveOutReset(hWaveOut);
	for(Cnt = 0x00; Cnt < AUDIOBUFFERU; Cnt++){
		waveOutUnprepareHeader(hWaveOut, &WaveHdrOut[Cnt], sizeof(WAVEHDR));
	}

	if(!SkipWOClose){
		RetVal = waveOutClose(hWaveOut);
	}else{
		RetVal = MMSYSERR_NOERROR;
	}
	if(RetVal != MMSYSERR_NOERROR){
		return 0xC4;    // waveOutClose failed  -- but why ???
	}

	return 0x00;
}

void PauseStream(bool PauseOn){
	if(!WaveOutOpen){
		return;
	}   // Thread is not active

	PauseThread = PauseOn;
}

static DWORD WINAPI WaveOutThread(void *Arg){
#ifdef NDEBUG
	uint32_t RetVal;
#endif
	uint16_t CurBuf;
	WAVE_16BS *TempBuf;

	hWaveOutThread = GetCurrentThread();
#ifdef NDEBUG
	RetVal = SetThreadPriority(hWaveOutThread, THREAD_PRIORITY_TIME_CRITICAL);
	if (! RetVal)
	{
		// Error by setting priority
		// try a lower priority, because too low priorities cause sound stuttering
		RetVal = SetThreadPriority(hWaveOutThread, THREAD_PRIORITY_HIGHEST);
	}
#endif

	BlocksSent = 0x00;
	BlocksPlayed = 0x00;
	while(!CloseThread){
		while(PauseThread && !CloseThread){
			Sleep(1);
		}
		if(CloseThread){
			break;
		}

		BufCheck();
		for(CurBuf = 0x00; CurBuf < AUDIOBUFFERU; CurBuf++){
			if(WaveHdrOut[CurBuf].dwFlags & WHDR_DONE){
				TempBuf = (WAVE_16BS *) WaveHdrOut[CurBuf].lpData;

				WaveHdrOut[CurBuf].dwUser |= 0x01;
				FillBuffer(TempBuf, SMPL_P_BUFFER);
				waveOutWrite(hWaveOut, &WaveHdrOut[CurBuf], sizeof(WAVEHDR));
				if(SoundLog && hFile != NULL){
					SaveFile(SMPL_P_BUFFER, TempBuf);
				}
				BlocksSent++;
				BufCheck();
			}
			if(CloseThread){
				break;
			}
		}
		Sleep(1);
	}

	hWaveOutThread = NULL;
	return 0x00000000;
}

static void BufCheck(){
	uint16_t CurBuf;

	for(CurBuf = 0x00; CurBuf < AUDIOBUFFERU; CurBuf++){
		if(WaveHdrOut[CurBuf].dwFlags & WHDR_DONE){
			if(WaveHdrOut[CurBuf].dwUser & 0x01){
				WaveHdrOut[CurBuf].dwUser &= ~0x01;
				BlocksPlayed++;
			}
		}
	}
}
