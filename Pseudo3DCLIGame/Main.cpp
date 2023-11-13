#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <stdio.h>
#include <Windows.h>

using namespace std;

void UpdateConsoleSize();

int ScreenWidth = 120;			// Console Screen Size X (columns)
int ScreenHeight = 40;			// Console Screen Size Y (rows)
int MapWidth = 0;				// World Dimensions
int MapHeight = 0;

float PlayerX = 14.7f;			// Player Start Position
float PlayerY = 5.09f;
float PlayerA = 0.0f;			// Player Start Rotation
float FOV = 3.14159f / 4.0f;	// Field of View
float Depth = 16.0f;			// Maximum rendering distance
float Speed = 5.0f;				// Walking Speed

int main()
{
	string mapFileName;
	cout << "Enter the name of the map file: ";
	cin >> mapFileName;
	system("cls");

	UpdateConsoleSize();

	// Create Screen Buffer
	wchar_t* screen = new wchar_t[ScreenWidth * ScreenHeight];
	HANDLE console = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
	SetConsoleActiveScreenBuffer(console);
	DWORD bytesWritten = 0;

	// Create Map of world space # = wall block, . = space
	wifstream file(mapFileName);
	wstring map, line;
	while (getline(file, line))
	{
		map += line;
		MapHeight++;
	}
	MapWidth = (int)(line.length());

	auto tp1 = chrono::system_clock::now();
	auto tp2 = chrono::system_clock::now();

	while (1)
	{
		// We'll need time differential per frame to calculate modification
		// to movement speeds, to ensure consistant movement, as ray-tracing
		// is non-deterministic
		tp2 = chrono::system_clock::now();
		chrono::duration<float> elapsedTime = tp2 - tp1;
		tp1 = tp2;
		float fElapsedTime = elapsedTime.count();

		// Handle CCW Rotation
		if (GetAsyncKeyState((unsigned short)'A') & 0x8000)
			PlayerA -= (Speed * 0.75f) * fElapsedTime;

		// Handle CW Rotation
		if (GetAsyncKeyState((unsigned short)'D') & 0x8000)
			PlayerA += (Speed * 0.75f) * fElapsedTime;

		// Handle Forwards movement & collision
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

		// Handle backwards movement & collision
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
			// For each column, calculate the projected ray angle into world space
			float rayAngle = (PlayerA - FOV / 2.0f) + ((float)x / (float)ScreenWidth) * FOV;

			// Find distance to wall
			float stepSize = 0.1f;			// Increment size for ray casting, decrease to increase resolution
			float distanceToWall = 0.0f;

			bool hitWall = false;	// Set when ray hits wall block
			bool boundary = false;	// Set when ray hits boundary between two wall blocks

			float eyeX = sinf(rayAngle); // Unit vector for ray in player space
			float eyeY = cosf(rayAngle);

			// Incrementally cast ray from player, along ray angle, testing for 
			// intersection with a block
			while (!hitWall && distanceToWall < Depth)
			{
				distanceToWall += stepSize;
				int testX = (int)(PlayerX + eyeX * distanceToWall);
				int testY = (int)(PlayerY + eyeY * distanceToWall);

				// Test if ray is out of bounds
				if (testX < 0 || testX >= MapWidth || testY < 0 || testY >= MapHeight)
				{
					hitWall = true;			// Just set distance to maximum depth
					distanceToWall = Depth;
				}
				else
				{
					// Ray is inbounds so test to see if the ray cell is a wall block
					if (map.c_str()[testX * MapWidth + testY] == '#')
					{
						// Ray has hit wall
						hitWall = true;

						// To highlight tile boundaries, cast a ray from each corner
						// of the tile, to the player. The more coincident this ray
						// is to the rendering ray, the closer we are to a tile 
						// boundary, which we'll shade to add detail to the walls
						vector<pair<float, float>> p;

						// Test each corner of hit tile, storing the distance from
						// the player, and the calculated dot product of the two rays
						for (int tx = 0; tx < 2; tx++)
						{
							for (int ty = 0; ty < 2; ty++)
							{
								// Angle of corner to eye
								float vy = (float)testY + ty - PlayerY;
								float vx = (float)testX + tx - PlayerX;
								float d = sqrt(vx * vx + vy * vy);
								float dot = (eyeX * vx / d) + (eyeY * vy / d);
								p.push_back(make_pair(d, dot));
							}
						}

						// Sort Pairs from closest to farthest
						sort(p.begin(), p.end(), [](const pair<float, float>& left, const pair<float, float>& right) {return left.first < right.first; });

						// First two/three are closest (we will never see all four)
						float bound = 0.005f;
						if (acos(p.at(0).second) < bound) boundary = true;
						if (acos(p.at(1).second) < bound) boundary = true;
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
		swprintf(screen, 50, L"X=%3.2f, Y=%3.2f, A=%3.2f FPS=%3.2f ", PlayerX, PlayerY, PlayerA, 1.0f / fElapsedTime);

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

void UpdateConsoleSize()
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	ScreenWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	ScreenHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
}