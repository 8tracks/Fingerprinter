/***************************************************************************
 * This file is part of last.fm fingerprint app                             *
 *  Last.fm Ltd <mir@last.fm>                                               *
 *                                                                          *
 * This library is free software; you can redistribute it and/or            *
 * modify it under the terms of the GNU Lesser General Public               *
 * License as published by the Free Software Foundation; either             *
 * version 2.1 of the License, or (at your option) any later version.       *
 *                                                                          *
 * This library is distributed in the hope that it will be useful,          *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *
 * Lesser General Public License for more details.                          *
 *                                                                          *
 * You should have received a copy of the GNU Lesser General Public         *
 * License along with this library; if not, write to the Free Software      *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 *
 * USA                                                                      *
 ***************************************************************************/

// for fingerprint
#include "../../fplib/include/FingerprintExtractor.h"

#include <sndfile.hh>

#include "MP3_Source.h" // to decode mp3s
#include "HTTPClient.h" // for connection

#include "Sha256File.h" // for SHA 256
#include "mbid_mp3.h"   // for musicbrainz ID

// taglib
#include <fileref.h>
#include <tag.h>
#include <id3v2tag.h>
#include <mpegfile.h>

// stl
#include <iostream>
#include <fstream>
#include <sstream>
#include <cctype> // for tolower
#include <algorithm>
#include <map>

using namespace std;

// hacky!
#ifdef WIN32
#define SLASH '\\'
#else
#define SLASH '/'
#endif

// DO NOT CHANGE THOSE!
const char FP_SERVER_NAME[]       = "ws.audioscrobbler.com/fingerprint/query/";
const char METADATA_SERVER_NAME[] = "http://ws.audioscrobbler.com/2.0/";
const char PUBLIC_CLIENT_NAME[]   = "fp client 1.6";
const char HTTP_POST_DATA_NAME[]  = "fpdata";

// if you want to use the last.fm fingerprint library in your app you'll need
// your own key
const char LASTFM_API_KEY[] = "2bfed60da64b96c16ea77adbf5fe1a82";

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// just turn it into a string. Similar to boost::lexical_cast
  template <typename T>
std::string toString(const T& val)
{
  ostringstream oss;
  oss << val;
  return oss.str();
}

// -----------------------------------------------------------------------------

bool plain_isspace(char c)
{
  if ( c == ' ' ||
      c == '\t' ||
      c == '\n' ||
      c == '\r' )
  {
    return true;
  }
  else
  {
    return false;
  }
}

// -----------------------------------------------------------------------------

string simpleTrim( const string& str )
{
  if ( str.empty() )
    return "";

  // left trim
  string::const_iterator lIt = str.begin();
  for ( ; plain_isspace(*lIt) && lIt != str.end(); ++lIt );
  if ( lIt == str.end() )
    return str;

  string::const_iterator rIt = str.end();
  --rIt;

  for ( ; plain_isspace(*rIt) && rIt != str.begin(); --rIt );
  ++rIt;

  return string(lIt, rIt);
}

// -----------------------------------------------------------------------------

void addEntry ( map<string, string>& urlParams, const string& key, const string& val )
{
  if ( key.empty() || val.empty() )
    return;
  if ( urlParams.find(key) != urlParams.end() )
    return; // do not add something that was already there

  urlParams[key] = simpleTrim(val);
}

// -----------------------------------------------------------------------------

// gather some data from the (mp3) file
void getFileInfo( const string& fileName, map<string, string>& urlParams, bool doTaglib = true )
{

  //////////////////////////////////////////////////////////////////////////
  // Musicbrainz ID
  char mbid_ch[MBID_BUFFER_SIZE];
  if ( getMP3_MBID(fileName.c_str(), mbid_ch) != -1 )
    urlParams["mbid"] = string(mbid_ch);

  //////////////////////////////////////////////////////////////////////////
  // Filename (optional)
  size_t lastSlash = fileName.find_last_of(SLASH);
  if ( lastSlash != string::npos )
    urlParams["filename"] = fileName.substr(lastSlash+1);
  else
    urlParams["filename"] = fileName;

  ///////////////////////////////////////////////////////////////////////////
  // SHA256

  const int SHA_SIZE = 32;
  unsigned char sha256[SHA_SIZE]; // 32 bytes
  Sha256File::getHash(fileName, sha256);

  urlParams["sha256"] = Sha256File::toHexString(sha256, SHA_SIZE);

  if ( !doTaglib )
    return;

  //////////////////////////////////////////////////////////////////////////
  // ID3

  try
  {
    TagLib::MPEG::File f(fileName.c_str());
    if( f.isValid() && f.tag() )
    {
      TagLib::Tag* pTag = f.tag();

      // artist
      addEntry( urlParams, "artist", string(pTag->artist().toCString(true)) );

      // album
      addEntry( urlParams, "album", string(pTag->album().toCString(true)) );

      // title
      addEntry( urlParams, "track", string(pTag->title().toCString(true)) );

      // track num
      if ( pTag->track() > 0 )
        addEntry( urlParams, "tracknum", toString(pTag->track()) );

      // year
      if ( pTag->year() > 0 )
        addEntry( urlParams, "year", toString(pTag->year()) );

      // genre
      addEntry( urlParams, "genre", string(pTag->genre().toCString(true)) );
    }
  }
  catch (const std::exception& /*e*/)
  {
    cerr << "WARNING: Taglib could not extract any information!" << endl;
  }
}

// -----------------------------------------------------------------------------

string fetchMetadata(int fpid, HTTPClient& client, bool justURL)
{
  ostringstream oss;
  oss << METADATA_SERVER_NAME
    << "?method=track.getfingerprintmetadata"
    << "&fingerprintid=" << fpid
    << "&api_key=" << LASTFM_API_KEY;

  if ( justURL )
    return oss.str();

  string c;
  // it's in there! let's get the metadata

  // try a couple of times max..
  for (int i = 0; i < 2 && c.empty(); ++i )
    c = client.get(oss.str());

  if ( c.empty() )
    cout << "The metadata server returned an empty page. Please try again later." << endl;

  return c;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
  if ( argc < 2 )
  {
    string fileName = string(argv[0]);
    size_t lastSlash = fileName.find_last_of(SLASH);
    if ( lastSlash != string::npos )
      fileName = fileName.substr(lastSlash+1);

    cout << fileName << " (" << PUBLIC_CLIENT_NAME << ")\n"
      << "A minimal fingerprint client, public release.\n"
      << "Copyright (C) 2007-2010 by Last.fm (MIR) - Build: " << __DATE__ << " (" << __TIME__ << ")\n\n"
      << "Usage:\n"
      << fileName << " [options] yourMp3File.mp3\n"
      << "Available options:\n"
      << " -nometadata: will only return the id of the fingerprint\n"
      << " -url: will output the url of the metadata\n";
    exit(0);
  }

  string mp3FileName;
  int duration, samplerate, bitrate, nchannels;

  bool wantMetadata = true;
  bool justUrl = false;

  bool doTagLib = true;
  int forceDuration = 0;

  string serverName = FP_SERVER_NAME;

  // man, I am so used to boost::program_options that I suck at writing command line parsers now

  for(int i = 1; i < argc; ++i )
  {
    if ( argv[i][0] != '-' )
    {
      mp3FileName = argv[i]; // assume it's the filename
      continue;
    }

    string arg(argv[i]);

    if ( arg == "-nometadata" )
      wantMetadata = false;
    else if ( arg == "-url" )
      justUrl = true;
    else if ( arg == "-fplocation" && (i+1) < argc )
    {

      ++i;
      serverName = argv[i] + string("/fingerprint/query/");
    }
    else if ( arg == "-nometadatanotaglib" ) // hidden
    {
      wantMetadata = false;
      doTagLib = false;
    }
    else if ( arg == "-length" && (i+1) < argc )
    {
      ++i;
      forceDuration = atoi(argv[i]);
      if ( forceDuration == 0 )
      {
        cout << "Invalid length parameter for -lenght <" << argv[i] << ">\n";
        exit(1);
      }
    }
    else
    {
      cout << "Invalid option or parameter <" << argv[i] << ">\n";
      exit(1);
    }
  }

  //////////////////////////////////////////////////////////////////////////

  {
    // check if it exists
    ifstream checkFile(mp3FileName.c_str(), ios::binary);
    if ( !checkFile.is_open() )
    {
      cerr << "ERROR: Cannot find file <" << mp3FileName << ">!" << endl;
      exit(1);
    }
  }

  map<std::string, std::string> urlParams;

  int duration, samplerate, bitrate, nchannels;

  // WARNING!!! This is absolutely mandatory!
  // If you don't specify the right duration you will not get the correct result!
  if ( forceDuration > 0 )
    urlParams["duration"] = toString(forceDuration);
  else
    urlParams["duration"] = toString(duration); // this is absolutely mandatory

  urlParams["username"]   = PUBLIC_CLIENT_NAME; // replace with username if possible
  urlParams["samplerate"] = toString(samplerate);

  // static short buffer [BUFFER_LEN] ;

  SndfileHandle infile ;
  infile = SndfileHandle(mp3FileName);

  // file.read (buffer, BUFFER_LEN) ;


  // This will extract the fingerprint
  // IMPORTANT: FingerprintExtractor assumes the data starts from the beginning of the file!
  fingerprint::FingerprintExtractor fextr;
  fextr.initForQuery(samplerate, nchannels, duration); // initialize for query

  size_t version = fextr.getVersion();
  // wow, that's odd.. If I god directly with getVersion I get a strange warning with VS2005.. :P
  urlParams["fpversion"]  = toString( version );

  // that's for the mp3
  // MP3_Source mp3Source;
  // the buffer can be any size, but FingerprintExtractor is happier (read: faster) with 2^x
  const size_t PCMBufSize = 131072;
  short* pPCMBuffer = new short[PCMBufSize];

  try
  {

    for (;;) {

      size_t read_data = infile.read(pPCMBuffer, PCMBufSize / 2);

      // cout << *pPCMBuffer << endl;

      if(fextr.process(pPCMBuffer, read_data)){
        break;
      }

    }

    // get the fingerprint data
    pair<const char*, size_t> fpData = fextr.getFingerprint();


    // Output URL PARAMS
    // cout << "URL params:" << endl;
    // map<std::string, std::string>::iterator i;
    // for(i = urlParams.begin(); i != urlParams.end(); i++){
    // cout << i->first << ": " << i->second << endl;
    // }
    // cout << endl;

    map<std::string, std::string> my_url_params;
    my_url_params["artist"]     = "Rod Stewart";
    my_url_params["duration"]   = "326";
    my_url_params["filename"]   = "Rod.mp3";
    my_url_params["fpversion"]  = "1";
    my_url_params["genre"]      = "Other";
    my_url_params["samplerate"] = "44100";
    my_url_params["sha256"]     = "fc4ef891e220ab751ee1328abaf1d40916ec1dba35dee4807a69001c9047f185";
    my_url_params["username"]   = "fp client 1.6";

    // send the fingerprint data, and get the fingerprint ID
    HTTPClient client;
    // string c = client.postRawObj( serverName, urlParams,
    // fpData.first, fpData.second,
    // HTTP_POST_DATA_NAME, false );
    string c = client.postRawObj( serverName, my_url_params,
        fpData.first, fpData.second,
        HTTP_POST_DATA_NAME, false );
    int fpid;
    istringstream iss(c);
    iss >> fpid;

    if ( !wantMetadata && iss.fail() )
      c = "-1 FAIL"; // let's keep it parseable: we had an error!
    else if ( wantMetadata && !iss.fail() )
    {
      // if there was no error and it wants metadata
      string state;
      iss >> state;
      if ( state == "FOUND" )
        c = fetchMetadata(fpid, client, justUrl);
      else if ( state == "NEW" )
      {
        cout << "Was not found! Now added, thanks! :)" << endl;
        c.clear();
      }
    }

    // if iss.fail() this will display the error, otherwise it will display
    // metadata or the id

    cout << c;

  }
  catch (const std::exception& e)
  {
    cerr << "ERROR: " << e.what() << endl;
    exit(1);
  }

  // clean up!
  delete [] pPCMBuffer;

  // bye bye and thanks for all the fish!
  return 0;
}

// -----------------------------------------------------------------------------
