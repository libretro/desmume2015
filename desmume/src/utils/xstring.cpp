//taken from fceux on 10/27/08
//subsequently modified for desmume

/*
	Copyright (C) 2008-2009 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "xstring.h"
#include <string>
#include <vector>

///Sub-String replacement routine

/// \brief convert input string into vector of string tokens
///
/// \note consecutive delimiters will be treated as single delimiter
/// \note delimiters are _not_ included in return data
///
/// \param input string to be parsed
/// \param delims list of delimiters.

std::vector<std::string> tokenize_str(const std::string & str,
                                      const std::string & delims=", \t")
{
  using namespace std;
  // Skip delims at beginning, find start of first token
  string::size_type lastPos = str.find_first_not_of(delims, 0);
  // Find next delimiter @ end of token
  string::size_type pos     = str.find_first_of(delims, lastPos);

  // output vector
  vector<string> tokens;

  while (string::npos != pos || string::npos != lastPos)
    {
      // Found a token, add it to the vector.
      tokens.push_back(str.substr(lastPos, pos - lastPos));
      // Skip delims.  Note the "not_of". this is beginning of token
      lastPos = str.find_first_not_of(delims, pos);
      // Find next delimiter at end of token.
      pos     = str.find_first_of(delims, lastPos);
    }

  return tokens;
}

std::string stditoa(int n)
{
	char tempbuf[16];
	sprintf(tempbuf, "%d", n);
	return tempbuf;
}

// replace all instances of victim with replacement
std::string mass_replace(const std::string &source, const std::string &victim, const std::string &replacement)
{
	std::string answer = source;
	std::string::size_type j = 0;
	while ((j = answer.find(victim, j)) != std::string::npos )
	{
		answer.replace(j, victim.length(), replacement);
		j+= replacement.length();
	}
	return answer;
}

//http://www.codeproject.com/KB/string/UtfConverter.aspx
#include "ConvertUTF.h"
namespace UtfConverter
{
    static std::wstring FromUtf8(const std::string& utf8string)
    {
        size_t widesize = utf8string.length();
        if (sizeof(wchar_t) == 2)
        {
            wchar_t* widestringnative = new wchar_t[widesize+1];
            const UTF8* sourcestart = reinterpret_cast<const UTF8*>(utf8string.c_str());
            const UTF8* sourceend = sourcestart + widesize;
            UTF16* targetstart = reinterpret_cast<UTF16*>(widestringnative);
            UTF16* targetend = targetstart + widesize+1;
            ConversionResult res = ConvertUTF8toUTF16(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
            if (res != conversionOK)
            {
                delete [] widestringnative;
                throw std::exception();
            }
            *targetstart = 0;
            std::wstring resultstring(widestringnative);
            delete [] widestringnative;
            return resultstring;
        }
        else if (sizeof(wchar_t) == 4)
        {
            wchar_t* widestringnative = new wchar_t[widesize+1];
            const UTF8* sourcestart = reinterpret_cast<const UTF8*>(utf8string.c_str());
            const UTF8* sourceend = sourcestart + widesize;
            UTF32* targetstart = reinterpret_cast<UTF32*>(widestringnative);
            UTF32* targetend = targetstart + widesize+1;
            ConversionResult res = ConvertUTF8toUTF32(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
            if (res != conversionOK)
            {
                delete [] widestringnative;
                throw std::exception();
            }
            *targetstart = 0;
            std::wstring resultstring(widestringnative);
            delete [] widestringnative;
            return resultstring;
        }
        else
        {
            throw std::exception();
        }
        return L"";
    }

    static std::string ToUtf8(const std::wstring& widestring)
    {
        size_t widesize = widestring.length();

        if (sizeof(wchar_t) == 2)
        {
            size_t utf8size = 3 * widesize + 1;
            char* utf8stringnative = new char[utf8size];
            const UTF16* sourcestart = reinterpret_cast<const UTF16*>(widestring.c_str());
            const UTF16* sourceend = sourcestart + widesize;
            UTF8* targetstart = reinterpret_cast<UTF8*>(utf8stringnative);
            UTF8* targetend = targetstart + utf8size;
            ConversionResult res = ConvertUTF16toUTF8(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
            if (res != conversionOK)
            {
                delete [] utf8stringnative;
                throw std::exception();
            }
            *targetstart = 0;
            std::string resultstring(utf8stringnative);
            delete [] utf8stringnative;
            return resultstring;
        }
        else if (sizeof(wchar_t) == 4)
        {
            size_t utf8size = 4 * widesize + 1;
            char* utf8stringnative = new char[utf8size];
            const UTF32* sourcestart = reinterpret_cast<const UTF32*>(widestring.c_str());
            const UTF32* sourceend = sourcestart + widesize;
            UTF8* targetstart = reinterpret_cast<UTF8*>(utf8stringnative);
            UTF8* targetend = targetstart + utf8size;
            ConversionResult res = ConvertUTF32toUTF8(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
            if (res != conversionOK)
            {
                delete [] utf8stringnative;
                throw std::exception();
            }
            *targetstart = 0;
            std::string resultstring(utf8stringnative);
            delete [] utf8stringnative;
            return resultstring;
        }
        else
        {
            throw std::exception();
        }
        return "";
    }
}
  
//convert a std::string to std::wstring
std::wstring mbstowcs(std::string str)
{
	try {
		return UtfConverter::FromUtf8(str);
	} catch(std::exception) {
		return L"(failed UTF-8 conversion)";
	}
}

std::string wcstombs(std::wstring str)
{
	return UtfConverter::ToUtf8(str);
}
