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
#include <cmath>

//#include "MP3_Source.h" // to decode mp3s
#include "HTTPClient.h" // for connection

//#include "Sha256File.h" // for SHA 256
//#include "mbid_mp3.h"   // for musicbrainz ID

// taglib
//#include <fileref.h>
//#include <tag.h>
//#include <id3v2tag.h>
//#include <mpegfile.h>

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
      << "Usage:\n"
      << fileName << " [options] yourMp3File.mp3\n\n"
      << "REQUIRED OPTIONS:\n"
      << " -duration: length of song in seconds\n"
      << " -samplerate: something like 44100\n"
      << " -channels: # of channels (1 or 2)\n";
    exit(0);
  }

  string wav_file_name;
  int duration, samplerate, nchannels;
  string artist;
  string filename;
  string sha256;
  string genre;
  map<std::string, std::string> urlParams;

  /* Old vars used in the mp3 version of this code */
  bool wantMetadata = true;
  bool justUrl = false;

  bool debug = false;

  string serverName = FP_SERVER_NAME;

  // man, I am so used to boost::program_options that I suck at writing command line parsers now

  for(int i = 1; i < argc; ++i )
  {
    if ( argv[i][0] != '-' )
    {
      wav_file_name = argv[i]; // assume it's the filename
      continue;
    }

    string arg(argv[i]);

    if(arg == "-duration" && (i+1) < argc) {
      duration = atoi(argv[++i]);
    }
    else if(arg == "-samplerate" && (i+1) < argc) {
      samplerate = atoi(argv[++i]);
    }
    else if(arg == "-channels" && (i+1) < argc) {
      nchannels = atoi(argv[++i]);
    }
    // Metadata
    else if(arg == "-artist" && (i+1) < argc) {
      urlParams["artist"] = argv[++i];
    }
    else if(arg == "-album" && (i+1) < argc) {
      urlParams["album"] = argv[++i];
    }
    else if(arg == "-track" && (i+1) < argc) {
      urlParams["track"] = argv[++i];
    }
    else if(arg == "-tracknum" && (i+1) < argc) {
      urlParams["tracknum"] = argv[++i];
    }
    else if(arg == "-year" && (i+1) < argc) {
      urlParams["year"] = argv[++i];
    }
    else if(arg == "-genre" && (i+1) < argc) {
      urlParams["genre"] = argv[++i];
    }
    else if(arg == "-filename" && (i+1) < argc) {
      // This is the original mp3 file name if we have it
      urlParams["filename"] = argv[++i];
    }
    else if(arg == "-sha" && (i+1) < argc) {
      urlParams["sha256"] = argv[++i];
    }
    else if(arg == "-debug") {
      debug = true;
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
    ifstream checkFile(wav_file_name.c_str(), ios::binary);
    if ( !checkFile.is_open() )
    {
      cerr << "ERROR: Cannot find file <" << wav_file_name << ">!" << endl;
      exit(1);
    }
  }


  SndfileHandle infile ;
  infile = SndfileHandle(wav_file_name);

  nchannels = infile.channels();
  samplerate = infile.samplerate();
  duration = ceil(infile.frames() / samplerate);

  // WARNING!!! This is absolutely mandatory!
  // If you don't specify the right duration you will not get the correct result!
  // This has to be passed in.
  urlParams["duration"]   = toString(duration); // this is absolutely mandatory
  urlParams["username"]   = PUBLIC_CLIENT_NAME; // replace with username if possible
  urlParams["samplerate"] = toString(samplerate);


  // This will extract the fingerprint
  // IMPORTANT: FingerprintExtractor assumes the data starts from the beginning of the file!
  fingerprint::FingerprintExtractor fextr;
  fextr.initForQuery(samplerate, nchannels, duration); // initialize for query

  size_t version = fextr.getVersion();
  // wow, that's odd.. If I god directly with getVersion I get a strange warning with VS2005.. :P
  urlParams["fpversion"]  = toString( version );

  // MP3_Source mp3Source;
  // the buffer can be any size, but FingerprintExtractor is happier (read: faster) with 2^x
  //
  // NOTE: This was left from the previous version of this code.
  const size_t PCMBufSize = 131072;
  short* pPCMBuffer = new short[PCMBufSize];

  try
  {

    for (;;) {

      size_t read_data = infile.read(pPCMBuffer, PCMBufSize / nchannels);
      if(fextr.process(pPCMBuffer, read_data)){
        break;
      }

    }

    // get the fingerprint data
    pair<const char*, size_t> fpData = fextr.getFingerprint();


    // Output URL PARAMS
    if(debug) {
      cout << "URL params:" << endl;
      map<std::string, std::string>::iterator i;
      for(i = urlParams.begin(); i != urlParams.end(); i++){
        cout << i->first << ": " << i->second << endl;
      }
      cout << endl;
    }

    // send the fingerprint data, and get the fingerprint ID
    HTTPClient client;
    string c = client.postRawObj(
      serverName, urlParams, fpData.first, fpData.second, HTTP_POST_DATA_NAME, false);

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
