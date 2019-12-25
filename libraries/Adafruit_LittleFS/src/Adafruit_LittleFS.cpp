/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <Arduino.h>
#include <string.h>
#include "Adafruit_LittleFS.h"

using namespace Adafruit_LittleFS_Namespace;


//--------------------------------------------------------------------+
// Implementation
//--------------------------------------------------------------------+

Adafruit_LittleFS::Adafruit_LittleFS (void)
  : Adafruit_LittleFS(NULL)
{

}

Adafruit_LittleFS::Adafruit_LittleFS (struct lfs_config* cfg)
{
  varclr(&_lfs);
  _lfs_cfg = cfg;
  _mounted = false;
  _mutex = xSemaphoreCreateMutexStatic(&this->xMutexStorageSpace);
}

Adafruit_LittleFS::~Adafruit_LittleFS ()
{

}

// Initialize and mount the file system
// Return true if mounted successfully else probably corrupted.
// User should format the disk and try again
bool Adafruit_LittleFS::begin (struct lfs_config * cfg)
{
  if ( _mounted ) return true;

  if (cfg) _lfs_cfg = cfg;
  if (!_lfs_cfg) return false;

  xSemaphoreTake(_mutex,  portMAX_DELAY);
  int err = lfs_mount(&_lfs, _lfs_cfg);
  xSemaphoreGive(_mutex);

  PRINT_LFS_ERR(err);
  _mounted = (err == LFS_ERR_OK);
  return _mounted;
}

// Tear down and unmount file system
void Adafruit_LittleFS::end(void)
{
  if (!_mounted) return;

  _mounted = false;

  xSemaphoreTake(_mutex,  portMAX_DELAY);
  int err = lfs_unmount(&_lfs);
  xSemaphoreGive(_mutex);

  PRINT_LFS_ERR(err);
  (void) err;
}

bool Adafruit_LittleFS::format (void)
{
  while (pdTRUE != xSemaphoreTake(_mutex,  portMAX_DELAY)) {}
  bool retval = this->xWrap_format();
  xSemaphoreGive(_mutex);
  return retval;
}

bool Adafruit_LittleFS::xWrap_format (void)
{
  xSemaphoreTake(_mutex,  portMAX_DELAY);

  // if already mounted: umount first -> format -> remount
  if(_mounted) VERIFY_LFS(lfs_unmount(&_lfs), false);

  VERIFY_LFS(lfs_format(&_lfs, _lfs_cfg), false);

  if (_mounted) VERIFY_LFS(lfs_mount(&_lfs, _lfs_cfg), false);

  xSemaphoreGive(_mutex);

  return true;
}

// Open a file or folder
Adafruit_LittleFS_Namespace::File Adafruit_LittleFS::open (char const *filepath, uint8_t mode)
{
  xSemaphoreTake(_mutex,  portMAX_DELAY);
  Adafruit_LittleFS_Namespace::File file(filepath, mode, *this);
  xSemaphoreGive(_mutex);

  return file;
}

// Check if file or folder exists
bool Adafruit_LittleFS::exists (char const *filepath)
{
  struct lfs_info info;

  xSemaphoreTake(_mutex,  portMAX_DELAY);
  int err = lfs_stat(&_lfs, filepath, &info);
  xSemaphoreGive(_mutex);

  PRINT_LFS_ERR(err);
  return err == LFS_ERR_OK;
}

// Create a directory, create intermediate parent if needed
bool Adafruit_LittleFS::mkdir (char const *filepath)
{
  bool ret = true;

  const char* slash = filepath;
  if ( slash[0] == '/' ) slash++;    // skip root '/'

  xSemaphoreTake(_mutex,  portMAX_DELAY);

  while ( NULL != (slash = strchr(slash, '/')) )
  {
    char parent[slash - filepath + 1] = { 0 };
    memcpy(parent, filepath, slash - filepath);

    // make intermediate parent if not existed
    int rc = lfs_mkdir(&_lfs, parent);
    if ( rc != LFS_ERR_OK && rc != LFS_ERR_EXIST )
    {
      // exit if failed to create parent
      PRINT_LFS_ERR(rc);
      ret = false;
      break;
    }

    slash++;
  }
  
  if (ret)
  {
    int rc = lfs_mkdir(&_lfs, filepath);
    if ( rc != LFS_ERR_OK && rc != LFS_ERR_EXIST )
    {
      PRINT_LFS_ERR(rc);
      ret = false;
    }
  }

  xSemaphoreGive(_mutex);

  return ret;
}

// Remove a file
bool Adafruit_LittleFS::remove (char const *filepath)
{
  xSemaphoreTake(_mutex,  portMAX_DELAY);
  int err = lfs_remove(&_lfs, filepath);
  xSemaphoreGive(_mutex);

  PRINT_LFS_ERR(err);
  return err == LFS_ERR_OK;
}

// Remove a folder
bool Adafruit_LittleFS::rmdir (char const *filepath)
{
  xSemaphoreTake(_mutex,  portMAX_DELAY);
  int err = lfs_remove(&_lfs, filepath);
  xSemaphoreGive(_mutex);

  PRINT_LFS_ERR(err);
  return err == LFS_ERR_OK;
}

// Remove a folder recursively
bool Adafruit_LittleFS::rmdir_r (char const *filepath)
{
  /* adafruit: lfs is modified to remove non-empty folder,
   According to below issue, comment these 2 line won't corrupt filesystem
   https://github.com/ARMmbed/littlefs/issues/43 */
  xSemaphoreTake(_mutex,  portMAX_DELAY);
  int err = lfs_remove(&_lfs, filepath);
  xSemaphoreGive(_mutex);

  PRINT_LFS_ERR(err);
  return err == LFS_ERR_OK;
}

//------------- Debug -------------//
#if CFG_DEBUG

const char* dbg_strerr_lfs (int32_t err)
{
  switch ( err )
  {
    case LFS_ERR_OK       : return "LFS_ERR_OK";
    case LFS_ERR_IO       : return "LFS_ERR_IO";
    case LFS_ERR_CORRUPT  : return "LFS_ERR_CORRUPT";
    case LFS_ERR_NOENT    : return "LFS_ERR_NOENT";
    case LFS_ERR_EXIST    : return "LFS_ERR_EXIST";
    case LFS_ERR_NOTDIR   : return "LFS_ERR_NOTDIR";
    case LFS_ERR_ISDIR    : return "LFS_ERR_ISDIR";
    case LFS_ERR_NOTEMPTY : return "LFS_ERR_NOTEMPTY";
    case LFS_ERR_BADF     : return "LFS_ERR_BADF";
    case LFS_ERR_INVAL    : return "LFS_ERR_INVAL";
    case LFS_ERR_NOSPC    : return "LFS_ERR_NOSPC";
    case LFS_ERR_NOMEM    : return "LFS_ERR_NOMEM";

    default:
      static char errcode[10];
      sprintf(errcode, "%ld", err);
      return errcode;
  }

  return NULL;
}

#endif
