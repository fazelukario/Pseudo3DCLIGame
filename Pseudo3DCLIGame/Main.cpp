#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <chrono>
#include <stdio.h>
#include <Windows.h>

using namespace std;

int ScreenWidth = 120;			// Console Screen Size X (columns)
int ScreenHeight = 40;			// Console Screen Size Y (rows)
int MapWidth = 16;				// World Dimensions
int MapHeight = 16;

float PlayerX = 14.7f;			// Player Start Position
float PlayerY = 5.09f;
float PlayerA = 0.0f;			// Player Start Rotation
float FOV = 3.14159f / 4.0f;	// Field of View
float Depth = 16.0f;			// Maximum rendering distance
float Speed = 5.0f;				// Walking Speed

int main()
{
	// Create Screen Buffer
	wchar_t* screen = new wchar_t[ScreenWidth * ScreenHeight];
	HANDLE console = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
	SetConsoleActiveScreenBuffer(console);
	DWORD bytesWritten = 0;

	// Create Map of world space # = wall block, . = space
	wstring map;
	map += L"################";
	map += L"#..............#";
	map += L"###.....########";
	map += L"#..............#";
	map += L"#......##......#";
	map += L"#......##......#";
	map += L"#..............#";
	map += L"###...........##";
	map += L"##.............#";
	map += L"#......####..###";
	map += L"#......#.......#";
	map += L"#......#.......#";
	map += L"#..............#";
	map += L"#......#########";
	map += L"#..............#";
	map += L"################";

	auto tp1 = chrono::system_clock::now();
	auto tp2 = chrono::system_clock::now();

	while (1)
	{
		tp2 = chrono::system_clock::now();
		chrono::duration<float> elapsedTime = tp2 - tp1;
		tp1 = tp2;
		float fElapsedTime = elapsedTime.count();


		if (GetAsyncKeyState((unsigned short)'A') & 0x8000)
			PlayerA -= (Speed * 0.75f) * fElapsedTime;

		if (GetAsyncKeyState((unsigned short)'D') & 0x8000)
			PlayerA += (Speed * 0.75f) * fElapsedTime;

		if (GetAsyncKeyState((unsigned short)'W') & 0x8000)
		{
			PlayerX += sinf(PlayerA) * Speed * fElapsedTime;
			PlayerY += cosf(PlayerA) * Speed * fElapsedTime;
			if (map.c_str()[(int)PlayerX * MapWidth + (int)PlayerY] == '#')
			{
				PlayerX -= sinf(PlayerA) * Speed * fElapsedTime;
				PlayerY -= cosf(PlayerA) * Speed * fElapsedTime;
			}
		}

		if (GetAsyncKeyState((unsigned short)'S') & 0x8000)
		{
			PlayerX -= sinf(PlayerA) * Speed * fElapsedTime;
			PlayerY -= cosf(PlayerA) * Speed * fElapsedTime;
			if (map.c_str()[(int)PlayerX * MapWidth + (int)PlayerY] == '#')
			{
				PlayerX += sinf(PlayerA) * Speed * fElapsedTime;
				PlayerY += cosf(PlayerA) * Speed * fElapsedTime;
			}
		}

		for (int x = 0; x < ScreenWidth; x++)
		{
			float rayAngle = (PlayerA - FOV / 2.0f) + ((float)x / (float)ScreenWidth) * FOV;

			// Find distance to wall
			float stepSize = 0.1f;									
			float distanceToWall = 0.0f;

			bool hitWall = false;
			bool boundary = false;

			float eyeX = sinf(rayAngle);
			float eyeY = cosf(rayAngle);

			while (!hitWall && distanceToWall < Depth)
			{
				distanceToWall += stepSize;
				int testX = (int)(PlayerX + eyeX * distanceToWall);
				int testY = (int)(PlayerY + eyeY * distanceToWall);

				if (testX < 0 || testX >= MapWidth || testY < 0 || testY >= MapHeight)
				{
					hitWall = true;
					distanceToWall = Depth;
				}
				else
				{
					if (map.c_str()[testX * MapWidth + testY] == '#')
					{
						// Ray has hit wall
						hitWall = true;

						vector<pair<float, float>> p;

						for (int tx = 0; tx < 2; tx++)
						{
							for (int ty = 0; ty < 2; ty++)
							{
								float vy = (float)testY + ty - PlayerY;
								float vx = (float)testX + tx - PlayerX;
								float d = sqrt(vx * vx + vy * vy);
								float dot = (eyeX * vx / d) + (eyeY * vy / d);
								p.push_back(make_pair(d, dot));
							}
						}

						// Sort Pairs from closest to farthest
						sort(p.begin(), p.end(), [](const pair<float, float>& left, const pair<float, float>& right) {return left.first < right.first; });

						float bound = 0.005f;
						if (acos(p.at(0).second) < bound) boundary = true;
						if (acos(p.at(1).second) < bound) boundary = true;
						if (acos(p.at(2).second) < bound) boundary = true;
					}
				}
			}

			// Calculate distance to ceiling and floor
			int ceiling = (int)((float)(ScreenHeight / 2.0f) - (float)(ScreenHeight / distanceToWall));
			int floor = ScreenHeight - ceiling;

			// Shader walls based on distance
			short shade = ' ';
			if (distanceToWall <= Depth / 4.0f)
				shade = 0x2588;	// Very close	
			else if (distanceToWall < Depth / 3.0f)
				shade = 0x2593;
			else if (distanceToWall < Depth / 2.0f)
				shade = 0x2592;
			else if (distanceToWall < Depth)
				shade = 0x2591;
			else
				shade = ' '; // Too far away

			if (boundary)
				shade = '|'; // Black it out

			for (int y = 0; y < ScreenHeight; y++)
			{
				// Each Row
				if (y <= ceiling)
					screen[y * ScreenWidth + x] = ' ';
				else if (y > ceiling && y <= floor)
					screen[y * ScreenWidth + x] = shade;
				else // Floor
				{
					// Shade floor based on distance
					float b = 1.0f - (((float)y - ScreenHeight / 2.0f) / ((float)ScreenHeight / 2.0f));
					if (b < 0.25)
						shade = '#';
					else if (b < 0.5)
						shade = 'x';
					else if (b < 0.75)
						shade = '.';
					else if (b < 0.9)
						shade = '-';
					else
						shade = ' ';
					screen[y * ScreenWidth + x] = shade;
				}
			}
		}

		// Display Stats
		swprintf(screen, 40, L"X=%3.2f, Y=%3.2f, A=%3.2f FPS=%3.2f ", PlayerX, PlayerY, PlayerA, 1.0f / fElapsedTime);

		// Display Map
		for (int nx = 0; nx < MapWidth; nx++)
		{
			for (int ny = 0; ny < MapWidth; ny++)
			{
				screen[(ny + 1) * ScreenWidth + nx] = map[ny * MapWidth + nx];
			}
		}
		screen[((int)PlayerX + 1) * ScreenWidth + (int)PlayerY] = 'P';

		// Display Frame
		screen[ScreenWidth * ScreenHeight - 1] = '\0';
		WriteConsoleOutputCharacter(console, screen, ScreenWidth * ScreenHeight, { 0,0 }, &bytesWritten);
	}

	return 0;
}