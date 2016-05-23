// ddconvert
//
// usage : ddconvert input output
// Converts a dd image from LBA ordering to track ordering
// Output format is Head 0, Zones 0-7 (1175 tracks total)
// followed by Head 1, Zones 0-7 (1175 tracks total)
// The output does not contain C1 bytes in the sectors,
// or C2 sectors in the blocks or GAP sectors in the blocks.
// Output does have block interleave for correct single block access
// and empty alternate tracks for correct track indexing.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTORS_PER_BLOCK	85
#define BLOCKS_PER_TRACK	2

#define BLOCKSIZE(_zone) ZoneSecSize[_zone] * SECTORS_PER_BLOCK
#define TRACKSIZE(_zone) BLOCKSIZE(_zone) * BLOCKS_PER_TRACK
#define ZONESIZE(_zone) TRACKSIZE(_zone) * ZoneTracks[_zone]
#define VZONESIZE(_zone) TRACKSIZE(_zone) * (ZoneTracks[_zone] - 0xC)
const unsigned int ZoneSecSize[16] = {232,216,208,192,176,160,144,128,
                                      216,208,192,176,160,144,128,112};
const unsigned int ZoneTracks[16] = {158,158,149,149,149,149,149,114,
                                     158,158,149,149,149,149,149,114};
const unsigned int DiskTypeZones[7][16] = {
	{0, 1, 2, 9, 8, 3, 4, 5, 6, 7,15,14,13,12,11,10},
	{0, 1, 2, 3,10, 9, 8, 4, 5, 6, 7,15,14,13,12,11},
	{0, 1, 2, 3, 4,11,10, 9, 8, 5, 6, 7,15,14,13,12},
	{0, 1, 2, 3, 4, 5,12,11,10, 9, 8, 6, 7,15,14,13},
	{0, 1, 2, 3, 4, 5, 6,13,12,11,10, 9, 8, 7,15,14},
	{0, 1, 2, 3, 4, 5, 6, 7,14,13,12,11,10, 9, 8,15},
	{0, 1, 2, 3, 4, 5, 6, 7,15,14,13,12,11,10, 9, 8}
	};
const unsigned int RevDiskTypeZones[7][16] = {
	{0, 1, 2, 5, 6, 7, 8, 9, 4, 3,15,14,13,12,11,10},
	{0, 1, 2, 3, 7, 8, 9,10, 6, 5, 4,15,14,13,12,11},
	{0, 1, 2, 3, 4, 9,10,11, 8, 7, 6, 5,15,14,13,12},
	{0, 1, 2, 3, 4, 5,11,12,10, 9, 8, 7, 6,15,14,13},
	{0, 1, 2, 3, 4, 5, 6,13,12,11,10, 9, 8, 7,15,14},
	{0, 1, 2, 3, 4, 5, 6, 7,14,13,12,11,10, 9, 8,15},
	{0, 1, 2, 3, 4, 5, 6, 7,15,14,13,12,11,10, 9, 8}
	};
const unsigned int StartBlock[7][16] = {
	{0,0,0,1,0,1,0,1,1,1,1,0,1,0,1,1},
	{0,0,0,1,1,0,1,0,1,1,0,1,0,1,0,0},
	{0,0,0,1,0,1,0,1,1,1,0,1,1,0,1,1},
	{0,0,0,1,0,1,1,0,1,1,0,1,0,1,0,0},
	{0,0,0,1,0,1,0,1,1,1,0,1,0,1,1,1},
	{0,0,0,1,0,1,0,1,0,0,1,0,1,0,1,0},
	{0,0,0,1,0,1,0,1,0,0,1,0,1,0,1,1}
};

const unsigned int StartTrack [16] = {  0,158,316,465,614,763, 912,1061, 		                              158,316,465,614,763,912,1061,1175};

int main(int argc, char* argv[])
{
	FILE* in;
	FILE* out;

	int disktype = 0;
	int zone,track,sector = 0;
	int head = 0;
	int atrack = 0;
	int block = 0;
	unsigned char SystemData[0xE8];
	unsigned char BlockData0[0x100*SECTORS_PER_BLOCK];
	unsigned char BlockData1[0x100*SECTORS_PER_BLOCK];
	unsigned int InOffset, OutOffset = 0;
	unsigned long InStart[16];
	unsigned long OutStart[16];

	InStart[0] = 0;
	OutStart[0] = 0;

	
	if(argc != 3)
		printf("Usage: ddconvert infile outfile\n");
	else
	{
		in = fopen(argv[1], "rb");
		out = fopen(argv[2], "wb");
		fseek(in, 0, SEEK_SET);
		fread((void *)(&SystemData), 0xE8, 1, in);

		disktype = SystemData[5] & 0xF;
	
		printf("Disk Type = %d\n", disktype);

		for(zone = 1; zone < 16; zone++)
		{
			InStart[zone] = InStart[zone-1] + 
				VZONESIZE(DiskTypeZones[disktype][zone-1]);
			printf("Input Offset %d: %x\n", zone, InStart[zone]);
		}

		for(zone = 1; zone < 16; zone++)
		{
			OutStart[zone] = OutStart[zone - 1] + ZONESIZE(zone - 1);
			printf("Output Offset %d: %x\n", zone, OutStart[zone]);
		}
	
		for(zone = 0; zone < 8; zone++)
		{
			OutOffset = OutStart[zone];
			InOffset = InStart[RevDiskTypeZones[disktype][zone]];
			fseek(in, InOffset, SEEK_SET);
			fseek(out, OutOffset, SEEK_SET);
			block = StartBlock[disktype][zone];
			atrack = 0;
			for(track = 0; track < ZoneTracks[zone]; track++)
			{
				if(atrack < 0xC && track == SystemData[0x20 + zone*0xC + atrack])
				{
					memset((void *)(&BlockData0), 0, BLOCKSIZE(zone));
					memset((void *)(&BlockData1), 0, BLOCKSIZE(zone));
					atrack += 1;
					printf("Alt Track, Zone %d Track %x\n", zone, track);
				}
				else
				{
					if((block % 2) == 1)
					{
						fread((void *)(&BlockData1), BLOCKSIZE(zone), 1, in);
						fread((void *)(&BlockData0), BLOCKSIZE(zone), 1, in);					
					}
					else
					{
						fread((void *)(&BlockData0), BLOCKSIZE(zone), 1, in);
						fread((void *)(&BlockData1), BLOCKSIZE(zone), 1, in);
					}
					block = 1 - block;
				}
				fwrite((void *)(&BlockData0), BLOCKSIZE(zone), 1, out);
				fwrite((void *)(&BlockData1), BLOCKSIZE(zone), 1, out);
			}
		}

		for(zone = 8; zone < 16; zone++)
		{
			//OutOffset = OutStart[zone];
			InOffset = InStart[RevDiskTypeZones[disktype][zone]];
			fseek(in, InOffset, SEEK_SET);
			//fseek(out, OutOffset, SEEK_SET);
			block = StartBlock[disktype][zone];
			atrack = 0xB;
			for(track = 1; track < ZoneTracks[zone] + 1; track++)
			{
				if(atrack > -1 && (ZoneTracks[zone] - track) == SystemData[0x20 + (zone)*0xC + atrack])
				{
					memset((void *)(&BlockData0), 0, BLOCKSIZE(zone));
					memset((void *)(&BlockData1), 0, BLOCKSIZE(zone));
					atrack -= 1;
					printf("Alt Track, Zone %d Track %x\n", zone, (ZoneTracks[zone] - track));
				}
				else
				{
					if((block % 2) == 1)
					{
						fread((void *)(&BlockData1), BLOCKSIZE(zone), 1, in);
						fread((void *)(&BlockData0), BLOCKSIZE(zone), 1, in);
					}
					else
					{
						fread((void *)(&BlockData0), BLOCKSIZE(zone), 1, in);
						fread((void *)(&BlockData1), BLOCKSIZE(zone), 1, in);
					}
					block = 1 - block;
				}
				OutOffset = OutStart[zone] + (ZoneTracks[zone] - track) * TRACKSIZE(zone);
				fseek(out, OutOffset, SEEK_SET);
				fwrite((void *)(&BlockData0), BLOCKSIZE(zone), 1, out);
				fwrite((void *)(&BlockData1), BLOCKSIZE(zone), 1, out);				
			}
		}

		fclose(in);
		fclose(out);
	}
	return 0;
}
