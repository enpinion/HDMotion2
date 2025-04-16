/**
 * @file main.c HDMotion for modern systems
 * @author Grant Kim
 * @brief Main entry point
 * @version 0.1
 * @date 2025-04-16
 *
 * @copyright Copyright (c) Grant K. 2025 All rights reserved - Released under
 * ISC License.
 * Original Code by Jeremy Stanley - The author of HDMotion
 *
 *    hdmotion - Moves hard disk heads in interesting patterns
   (C) 2005 by Jeremy Stanley

   This program may be distributed freely provided this notice
   is preserved.

   The author assumes no liability for any damages arising
   out of the use of this program, including but not limited
   to loss of data or desire to open up operational hard drives.
 *
 */

#include <math.h>   // rand()
#include <stdio.h>  // printf()
#include <stdlib.h> // srand();
#include <time.h>   // time()

#ifdef _WIN32
#  include <windows.h>
#endif // _WIN32

#if defined(__unix__) || defined(__linux__) || defined(BSD)
#  include <fcntl.h>
#endif // __unix__ || __linux__ || BSD

// Constants
#define PI 3.1415926535897932

// Global variables
static wchar_t g_ErrStringWin32[1024];

// Utils
#define NOISE (0.0001 * (rand() % 9 + 1))

/**
 * @brief Format LastError. Not thread-safe.
 *
 * @param LastError Error code to parse
 * @return const wchar_t* Error code string
 */
static const wchar_t *win32ErrorStr(DWORD LastError)
{
  FormatMessageW(
      FORMAT_MESSAGE_FROM_SYSTEM,
      NULL,
      LastError,
      0,
      g_ErrStringWin32,
      1000,
      NULL);

  WriteConsoleW(
      GetStdHandle(STD_OUTPUT_HANDLE),
      g_ErrStringWin32,
      wcsnlen(g_ErrStringWin32, 1024),
      NULL,
      NULL);

  return g_ErrStringWin32;
}

void move_head_win32(
    HANDLE        hDisk,
    LARGE_INTEGER diskSize,
    DWORD         bytesPerSector,
    double        position,
    void         *buffer,
    DWORD         bufferSize)
{
  // TODO: Should this be -1.0 to 1.0?
  // Clamp position to [0.0, 1.0]
  if ( position < 0.0 )
  {
    position = 0.0;
  }
  if ( position > 1.0 )
  {
    position = 1.0;
  }

  // Compute target byte offset
  LARGE_INTEGER offset;
  offset.QuadPart = (LONGLONG)(position * (diskSize.QuadPart - bufferSize));
  offset.QuadPart -= offset.QuadPart % 512;

  // Seek to the position
  if ( !SetFilePointerEx(hDisk, offset, NULL, FILE_BEGIN) )
  {
    printf("SetFilePointerEx failed: %lu\n", GetLastError());
    return;
  }

  // Read one sector to cause disk access
  DWORD bytesRead = 0;
  if ( !ReadFile(hDisk, buffer, bufferSize, &bytesRead, NULL) )
  {
    printf(
        "ReadFile failed at position %.3f: %lu\n%S\n",
        position,
        GetLastError(),
        win32ErrorStr(GetLastError()));
  }

  // Assume width is 80.
  // TODO: Use GetConsoleScreenBufferInfo() to handle its width.
  {
    char bar[80];
    memset(bar, ' ', 79);
    int mark  = (int)(position * 78);
    bar[mark] = '#';
    bar[79]   = '\0';
    puts(bar);
  }
}

typedef struct _DiskContext
{
  unsigned long long diskHandle; // Device handle. Discriptor
} DiskContext;

int main(int argc, const char *argv[])
{
  WINBOOL          winError      = TRUE;
  char             diskpath[512] = "\\\\.\\PhysicalDrive%d";
  DISK_GEOMETRY_EX diskgeom      = {};
  HANDLE           hDisk         = INVALID_HANDLE_VALUE;

  if ( argc < 2 )
  {
    puts("Missing disk ID");
    return 0;
  }

  srand(time(NULL));

  if ( snprintf(diskpath, 500, diskpath, atoi(argv[1])) < 0 )
  {
    puts("Parameter error");
    return -1;
  }

  hDisk = CreateFileA(
      diskpath,
      GENERIC_READ,
      FILE_SHARE_READ,
      NULL,
      OPEN_EXISTING,
      FILE_FLAG_NO_BUFFERING,
      NULL);

  if ( hDisk == INVALID_HANDLE_VALUE )
  {
    printf("Failed to open %s\n", diskpath);
    if ( GetLastError() == ERROR_ACCESS_DENIED )
    {
      printf(
          "%S\n"
          "This program requires administrator privilage.",
          win32ErrorStr(GetLastError()));
    }
    return -1;
  }

  winError = DeviceIoControl(
      hDisk,
      IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
      NULL,
      0,
      &diskgeom,
      sizeof(diskgeom),
      NULL,
      NULL);

  if ( !winError )
  {
    printf("Failed to acquire disk geometry\n");
  }

  SetFilePointer(hDisk, 0, 0, FILE_BEGIN);

  // Throw away buffer
  void *buffer = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_READWRITE);

  OVERLAPPED ov = {0};

  LARGE_INTEGER la;
  la = diskgeom.DiskSize;

  unsigned long long x =
      diskgeom.DiskSize.QuadPart / diskgeom.Geometry.BytesPerSector;

  LARGE_INTEGER current;
  LARGE_INTEGER laZero = {0};
  LARGE_INTEGER seek   = {0};
  DWORD         read;

  double f = 0.0;  // Fraction
  double s = 0.01; // Speed

#define ZIGZAG_STEP 5

  // Zig-Zag
  for ( int i = 0; i < ZIGZAG_STEP; ++i )
  {

    for ( f = 0.0; f <= 1.0; f += s + 0.001 * (i + 1) )
    {
      move_head_win32(
          hDisk,
          diskgeom.DiskSize,
          diskgeom.Geometry.BytesPerSector,
          f,
          buffer,
          512);
    }

    for ( f -= s; f >= 0.0; f -= s + 0.001 * (i + 1) )
    {
      move_head_win32(
          hDisk,
          diskgeom.DiskSize,
          diskgeom.Geometry.BytesPerSector,
          f,
          buffer,
          512);
      // printf("%f\n", f);
    }

    s += (ZIGZAG_STEP - 1) / ZIGZAG_STEP;
  }

  f += (ZIGZAG_STEP - 1) / ZIGZAG_STEP;

  // Tightening zigzag
  double h = 0.90;
  double l = 0.10;

  for ( ; l < h; )
  {
    for ( ; f < h; f += s + NOISE )
    {
      move_head_win32(
          hDisk,
          diskgeom.DiskSize,
          diskgeom.Geometry.BytesPerSector,
          f,
          buffer,
          512);
    }
    for ( ; f > l; f -= s + NOISE )
    {
      move_head_win32(
          hDisk,
          diskgeom.DiskSize,
          diskgeom.Geometry.BytesPerSector,
          f,
          buffer,
          512);
    }

    h -= 0.05;
    l += 0.05;
  }

  // Widening sinusoid

  double amp = 0.05;
  for ( ; amp <= 0.50; amp += 0.05 )
  {
    double x;
    for ( x = 0; x < 2 * PI; x += PI / 32.0 )
    {
      move_head_win32(
          hDisk,
          diskgeom.DiskSize,
          diskgeom.Geometry.BytesPerSector,
          sin(x) * amp + 0.5 + NOISE,
          buffer,
          512);
    }
  }

  // Narrowing sinusoid

  for ( amp = 0.50; amp > 0.0; amp -= 0.05 )
  {
    double x;
    for ( x = 0; x < 2 * PI; x += PI / 32.0 )
    {
      move_head_win32(
          hDisk,
          diskgeom.DiskSize,
          diskgeom.Geometry.BytesPerSector,
          sin(x) * amp + 0.5 + NOISE,
          buffer,
          512);
    }
  }

  // widening double-sinusoid
  amp = 0.05;
  for ( ; amp <= 0.5; amp += 0.05 )
  {
    double x;
    for ( x = 0; x < 2 * PI; x += PI / 16.0 )
    {
      f = sin(x) * amp + 0.5;
      move_head_win32(
          hDisk,
          diskgeom.DiskSize,
          diskgeom.Geometry.BytesPerSector,
          f + NOISE,
          buffer,
          512);

      move_head_win32(
          hDisk,
          diskgeom.DiskSize,
          diskgeom.Geometry.BytesPerSector,
          1.0 - f + NOISE,
          buffer,
          512);
    }
  }

  // narrowing double-sinusoid
  for ( amp = 0.50; amp >= 0.0; amp -= 0.05 )
  {
    double x;
    for ( x = 0; x < 2 * PI; x += PI / 16.0 )
    {
      f = sin(x) * amp + 0.5;
      move_head_win32(
          hDisk,
          diskgeom.DiskSize,
          diskgeom.Geometry.BytesPerSector,
          f + NOISE,
          buffer,
          512);

      move_head_win32(
          hDisk,
          diskgeom.DiskSize,
          diskgeom.Geometry.BytesPerSector,
          1.0 - f + NOISE,
          buffer,
          512);
    }
  }

  // buncha heads
  int heads = 0;
  for ( heads = 2; heads < 7; ++heads )
  {
    int repeat = 160 / heads;
    for ( int i = 0; i < repeat; ++i )
    {
      for ( int j = 1; j <= heads; ++j )
      {
        move_head_win32(
            hDisk,
            diskgeom.DiskSize,
            diskgeom.Geometry.BytesPerSector,
            (double)j / (heads + 1) + NOISE,
            buffer,
            512);
      }
    }
  }
  for ( ; heads > 0; heads -= 2 )
  {
    int repeat = 160 / heads;
    for ( int i = 0; i < repeat; ++i )
    {
      for ( int j = 1; j <= heads; ++j )
      {
        move_head_win32(
            hDisk,
            diskgeom.DiskSize,
            diskgeom.Geometry.BytesPerSector,
            (double)j / (heads + 1) + NOISE,
            buffer,
            512);
      }
    }
  }

  for ( int i = 0; i < 600; ++i )
  {
    move_head_win32(
        hDisk,
        diskgeom.DiskSize,
        diskgeom.Geometry.BytesPerSector,
        (double)rand() / RAND_MAX,
        buffer,
        512);
  }

  CloseHandle(hDisk);
}
