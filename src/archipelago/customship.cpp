#include <filesystem>
#include <algorithm>
#include <vector>
#include <iostream>
#include <cstdio>

#include "SDL_endian.h"

extern "C" {
	// Function definitions that go out C side need to not be mangled by C++
	#include "customship.h"
}

bool useCustomShips = true;
size_t currentCustomShip = 0;

// ------------------------------------------------------------------

// The maximum size for a ship (all rows filled, no duplicates, with text) is 4077 bytes.
#define MAX_SHIP_SPRITE_SIZE 4096

typedef struct StaticSpriteData {
public:
	Uint8 Data[MAX_SHIP_SPRITE_SIZE];
	char Name[40];
	char Author[40];

private:
	std::string Path;
	Sprite2_array GameSpriteArray;

	void ExtractStringField(char *destination, Uint16 fieldNum)
	{
		Uint16 headerFieldLoc = (fieldNum - 1) << 1;
		Uint16 realDataLoc;

		memcpy(&realDataLoc, &Data[headerFieldLoc], sizeof(Uint16));
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		SDL_Swap16(realDataLoc);
#endif

		if (realDataLoc > (4096 - 40) || Data[realDataLoc] != 0x0F)
			strncpy(destination, "Unknown", 40);
		else // String may not be null terminated, use memcpy
			memcpy(destination, &Data[realDataLoc + 1], 39);
		destination[39] = 0;
	}

public:
	bool operator<(const StaticSpriteData &other)
	{
		return strncmp(Name, other.Name, 39) < 0;
	}

	bool NameMatches(const char* cName)
	{
		return strncmp(Name, cName, 39) == 0;
	}

	bool PathMatches(const char* cPath)
	{
		return strcmp(Path.c_str(), cPath) == 0;
	}

	const char *GetPath(void)
	{
		return Path.c_str();
	}

	StaticSpriteData(std::string path)
	{
		Path = path;

		FILE *f = fopen(path.c_str(), "rb");
		if (!f)
			throw std::runtime_error("File doesn't exist");

		Uint16 lengthTest;
		size_t result = fread(&lengthTest, sizeof(Uint16), 1, f);
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		SDL_Swap16(lengthTest);
#endif

		if (result != 1 || lengthTest != 76)
		{
			fclose(f);
			throw std::runtime_error("Not recognized as ship file, ignoring");
		}

		fseek(f, 0, SEEK_SET);
		GameSpriteArray.size = fread(Data, 1, MAX_SHIP_SPRITE_SIZE, f);
		fclose(f);

		ExtractStringField(Name,   37);
		ExtractStringField(Author, 38);
	}

	Sprite2_array *AsSprite2Array(void)
	{
		GameSpriteArray.data = Data;
		return &GameSpriteArray;
	}
} StaticSpriteData;

static std::vector<StaticSpriteData> allShipData;

size_t CustomShip_SystemInit(const char *directories[], size_t num_directories)
{
	for (size_t i = 0; i < num_directories; ++i)
	{
		const std::filesystem::path ship_dir(directories[i]);
		for (auto const &entry : std::filesystem::recursive_directory_iterator(ship_dir))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".ship")
				continue;

			try
			{
				allShipData.emplace_back(entry.path().string());
			}
			catch (std::runtime_error &e)
			{
				std::cout << "warning: " << entry.path().string() << ": " << e.what() << std::endl;
			}
		}
	}

	std::sort(allShipData.begin(), allShipData.end());
	allShipData.shrink_to_fit();
	return allShipData.size();
}

size_t CustomShip_GetNumFromPath(const char *path)
{
	for (size_t i = 0; i < allShipData.size(); ++i)
	{
		if (allShipData[i].PathMatches(path))
			return i;
	}
	return (size_t)-1;
}

size_t CustomShip_GetNumFromName(const char *name)
{
	for (size_t i = 0; i < allShipData.size(); ++i)
	{
		if (allShipData[i].NameMatches(name))
			return i;
	}
	return (size_t)-1;
}

size_t CustomShip_Count(void)
{
	return allShipData.size();
}

bool CustomShip_Exists(size_t num)
{
	return (num < allShipData.size()); 
}

const char *CustomShip_GetPath(size_t num)
{
	return (num < allShipData.size()) ? allShipData[num].GetPath() : "";
}

const char *CustomShip_GetName(size_t num)
{
	return (num < allShipData.size()) ? allShipData[num].Name : "Unknown";
}

const char *CustomShip_GetAuthor(size_t num)
{
	return (num < allShipData.size()) ? allShipData[num].Author : "Unknown";
}

Sprite2_array *CustomShip_GetSprite(size_t num)
{
	return (num < allShipData.size()) ? allShipData[num].AsSprite2Array() : NULL;
}
