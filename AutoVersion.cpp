/*
* autoversion.cpp
* Copyright (C) 2024  T. Radde
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <conio.h>

#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
using namespace std;

#ifdef WIN32
	#include <windows.h>
#endif

#include "AutoVersion.h"


// ========================================================================
//                            Globals
// ========================================================================
bool g_bVerbose;	// program is verbose


// ========================================================================
//                            FindReplace
//
// This seems to be non-optimal at the first glance, but it is recursion
// safe, i.e. "a" can be safely replaced with "aa".
// ========================================================================
string FindReplace(const string &source, const string &pattern, const string &replace)
{
	size_t j = 0;
	string newstr;

	while (j + pattern.length() <= source.length())
	{
		if (source.substr(j, pattern.length()) == pattern)
		{
			newstr += replace;
			j += pattern.length();
		}
		else
			newstr += source[j++];
	}

	return newstr;
}


// ===============================================================================
//										Backup
//
// Creates .avbak rollback file
// ===============================================================================
void Backup(const string &file_name)
{
	string new_name = file_name + ".avbak";

	if (!CopyFile(file_name.c_str(), new_name.c_str(), FALSE))
		throw CException("can not create rollback file " + new_name);
}


// ===============================================================================
//										CanRollback
//
// tests, if a rollback is possible, i.e. if an .avbak file exists for the
// given file name
// ===============================================================================
bool CanRollback(const string &file_name)
{
	string bak = file_name + ".avbak";

	struct stat st;
	if (stat(bak.c_str(), &st) != 0)
		return false;
	
	return true;
}


// ===============================================================================
//										Rollback
//
// performs a rollback
// ===============================================================================
void Rollback(const string &file_name)
{
	string bak = file_name + ".avbak";

	if (g_bVerbose)
		printf("rolling back %s\n", bak.c_str());

	struct stat st;
	if (stat(bak.c_str(), &st) != 0)
		printf("ERROR: file %s does not exist! Rollback for this file not performed!\n", bak.c_str());
	else
	{
		if (_unlink(file_name.c_str()) != 0)
			printf("ERROR: can not delete file %s! Rollback for this file not performed!\n", file_name.c_str());
		else
		{
			if (rename(bak.c_str(), file_name.c_str()) != 0)
				printf("ERROR: can not rename file %s! Rollback for this file not performed!\n", bak.c_str());
		}
	}
}


// ===============================================================================
//							CReplace::CheckReplace
//
// checks, if a replacement will occur
// ===============================================================================
bool CReplace::CheckReplace(const string &file_name, char *buf, size_t size)
{
	size_t pos = 0;
	size_t what_len = m_strWhat.length();

	while (pos + what_len <= size)
	{
		if (strncmp(buf + pos, m_strWhat.c_str(), what_len) == 0)
		{
			if (m_strWhat == m_strWith)
				return false;

			m_bMustReplace = true;
			if (g_bVerbose)
				printf("%s: found '%s' (to be replaced with '%s')\n", file_name.c_str(), m_strWhat.c_str(), m_strWith.c_str());
			return true;
		}

		pos++;
	}

	// Der what-string MUSS gefunden werden, sonst stimmt etwas im Control File nicht
	throw CException(file_name + ": the string '" + m_strWhat + "' was not found!");
}


// ===============================================================================
//							CReplace::DoReplace
//
// performs a replacement for a file
// ===============================================================================
char *CReplace::DoReplace(char *buf, size_t &size)
{
	if (m_bMustReplace)
	{
		m_bDidReplace = true;

		size_t pos = 0;
		size_t what_len = m_strWhat.length();
		size_t with_len = m_strWith.length();

		while (pos + what_len <= size)
		{
			if (strncmp(buf + pos, m_strWhat.c_str(), what_len) == 0)
			{
				// found a replacement
				if (g_bVerbose)
					printf("replacing '%s' with '%s'\n", m_strWhat.c_str(), m_strWith.c_str());

				size_t newsize = size - what_len + with_len;
				char *newbuf = (char *)malloc(newsize);
				if (!newbuf)
					throw CException("out of memory");

				memcpy(newbuf, buf, pos);
				memcpy(newbuf + pos, m_strWith.c_str(), with_len);
				memcpy(newbuf + pos + with_len, buf + pos + what_len, size - pos - what_len);

				free(buf);
				buf = newbuf;
				size = newsize;
				pos += with_len;
			}
			else
				pos++;
		}
	}

	return buf;
}


// ===============================================================================
//							CReplace::UpdateControlFile
//
// Replacement auf das Control File anwenden
// ===============================================================================
char *CReplace::UpdateControlFile(char *buf, size_t &size, int &offset)
{
	if (m_bDidReplace)
	{
		// we must expand backslash to double-backslash and " to \"
		string what = FindReplace(m_strWhat, "\\", "\\\\");
		what = FindReplace(what, "\"", "\\\"");

		string with = FindReplace(m_strWith, "\\", "\\\\");
		with = FindReplace(with, "\"", "\\\"");

		size_t pos = m_nControlFilePos + offset;
		size_t what_len = what.length();
		size_t with_len = with.length();

		// printf("updating at pos = %ld (offset %ld) - >%s< with >%s<\n", pos, offset, what.c_str(), with.c_str());
		if (strncmp(buf + pos, what.c_str(), what_len) != 0)
			throw CException("updating control file failed! The what-string '" + what + "' was not found at the expected position!");

		size_t newsize = size - what_len + with_len;
		char *newbuf = (char *)malloc(newsize);
		if (!newbuf)
			throw CException("out of memory");

		memcpy(newbuf, buf, pos);
		memcpy(newbuf + pos, with.c_str(), with_len);
		memcpy(newbuf + pos + with_len, buf + pos + what_len, size - pos - what_len);

		free(buf);
		buf = newbuf;
		size = newsize;
		offset += (int)((int)with_len - (int)what_len);
	}

	return buf;
}


// ===============================================================================
//							CFileNode::CheckReplacements
//
// checks, if any replacement for this file will occur
// ===============================================================================
bool CFileNode::CheckReplacements(const string &file_name)
{
	if (g_bVerbose)
		printf("\nchecking file %s\n", file_name.c_str());

	// Testen, ob eine .avbak Datei für diese Datei existiert. Falls ja, dann Fehler.
	string bak = file_name + ".avbak";
	struct stat st;
	if (stat(bak.c_str(), &st) == 0)
		throw CException("the file " + bak + " already exists. Please perform a clean or a rollback first.");

	// Datei in den Speicher lesen
	if (stat(file_name.c_str(), &st) != 0)
		throw CException("stat failed for file " + file_name);

	size_t size = st.st_size;
	char *buf = (char *)malloc(size);
	if (!buf)
		throw CException("out of memory");

	FILE *fh = fopen(file_name.c_str(), "rb");
	size_t ret = fread(buf, 1, size, fh);
	fclose(fh);
	if (ret != size)
		throw CException("reading file " + file_name + " failed!");

	// Dann testen, ob ein Replacement durchgeführt wird, Replacements anzeigen.
	for (auto &it : m_listReplacements)
	{
		if (it.CheckReplace(file_name, buf, size))
			m_bMustReplace = true;
	}

	free(buf);
	return m_bMustReplace;
}


// ===============================================================================
//							CFileNode::DoReplacments
//
// performs all replacements for this file
// ===============================================================================
void CFileNode::DoReplacments(const string &file_name)
{
	if (m_bMustReplace)
	{
		if (g_bVerbose)
			printf("\nreplacing in file %s\n", file_name.c_str());

		// Datei in den Speicher lesen
		struct stat st;
		if (stat(file_name.c_str(), &st) != 0)
			throw CException("stat failed for file " + file_name);

		size_t size = st.st_size;
		char *buf = (char *)malloc(size);
		if (!buf)
			throw CException("out of memory");

		FILE *fh = fopen(file_name.c_str(), "rb");
		size_t ret = fread(buf, 1, size, fh);
		fclose(fh);
		if (ret != size)
			throw CException("reading file " + file_name + " failed!");

		// Replacements durchführen
		for (auto &it : m_listReplacements)
			buf = it.DoReplace(buf, size);

		// Backup erzeugen
		Backup(file_name);
		m_bDidReplace = true;

		// Datei schreiben
		fh = fopen(file_name.c_str(), "wb");
		int i = errno;
		if (!fh)
			throw CException("fopen for writing file " + file_name + " failed! " + strerror(errno));
		if (fwrite(buf, 1, size, fh) != size)
			throw CException("writing file " + file_name + " failed!");
		fclose(fh);

		free(buf);
	}
}


// ===============================================================================
//							CFileNode::Rollback
//
// performs a rollback for this file
// ===============================================================================
void CFileNode::Rollback(const string &file_name)
{
	if (m_bDidReplace)
		::Rollback(file_name);
}


// ===============================================================================
//							CAutoVersion::SkipWhiteSpaces
// ===============================================================================
void CAutoVersion::SkipWhiteSpaces(char *&p)
{
	while (*p == ' ' || *p == '\t')
		p++;
}


// ===============================================================================
//								CAutoVersion::SkipLine
// 
// only_white_spaces: if true, only white spaces and comments may appear
// ===============================================================================
void CAutoVersion::SkipLine(char *&p, bool only_white_spaces)
{
	bool is_comment = false;

	while (*p && *p != '\012' && *p != '\015')
	{
		if (*p == '#')
			is_comment = true;

		if (only_white_spaces && !is_comment && *p != ' ' && *p != '\t')
		{
			char c = *p;
			throw CParseException((string)"unexpected character '" + c + 
				(string)"'. Expected white-space or newline while scanning for end of line", m_nCurrentLine);
		}

		p++;
	}

	if (*p)
		m_nCurrentLine++;

	if (*p == '\015')	// Dos / Windows: 0x0d / 0x0a sequence
		p++;

	if (*p)
		p++;
}


// ===============================================================================
//								CAutoVersion::SkipComment
// ===============================================================================
void CAutoVersion::SkipComment(char *&p)
{
	while (*p && *p != '\012' && *p != '\015')
		p++;
}


// ===============================================================================
//								CAutoVersion::Scan
//
// Search for given string at the *beginning* of a new line.
// ===============================================================================
void CAutoVersion::Scan(char *&p, const char *token)
{
	size_t token_len = strlen(token);

	while (*p)
	{
		SkipLine(p, false);

		SkipWhiteSpaces(p);
		if (strncmp(p, token, token_len) == 0)
			return;
	}
}


// ===============================================================================
//							CAutoVersion::GetIdentifier
// ===============================================================================
string CAutoVersion::GetIdentifier(char *&p)
{
	SkipWhiteSpaces(p);

	const int MaxIdentLen = 256;
	char ident[MaxIdentLen + 1];

	int i = 0;
	while (*p && *p != ' ' && *p != '\t' && *p != '\012' && *p != '\015')
	{
		ident[i++] = *p++;
		if (i >= MaxIdentLen)
			throw CParseException("name for contant too long", m_nCurrentLine);
	}

	if (i == 0)
		throw CParseException("expected identifier", m_nCurrentLine);

	ident[i] = '\0';

	return ident;
}


// ===============================================================================
//							CAutoVersion::GetLiteral
//
// returns the position of the literal in "offset"
// ===============================================================================
string CAutoVersion::GetLiteral(char *&p, size_t *offset)
{
	SkipWhiteSpaces(p);

	if (*p++ != '"')
		throw CParseException("\" expected", m_nCurrentLine);

	if (offset)
		*offset = (p - m_pBuffer);

	const int MaxIdentLen = 256;
	char ident[MaxIdentLen + 1];

	int i = 0;
	while (*p && *p != '"' && *p != '\012' && *p != '\015')
	{
		if (*p == '\\' && *(p + 1) == '"')			// this is an escaped "
			p++;
		else if (*p == '\\' && *(p + 1) == '\\')	// this is an escaped backslash
			p++;

		ident[i++] = *p++;
		if (i >= MaxIdentLen)
			throw CParseException("literal too long", m_nCurrentLine);
	}

	if (*p++ != '"')
		throw CParseException("missing \"", m_nCurrentLine);

	if (i == 0)
		throw CParseException("empty literal not allowed", m_nCurrentLine);

	ident[i] = '\0';

	return ident;
}


// ===============================================================================
//							CAutoVersion::GetLiteralOrSymbol
// ===============================================================================
string CAutoVersion::GetLiteralOrSymbol(char *&p)
{
	SkipWhiteSpaces(p);

	if (*p == '"')
		return GetLiteral(p);

	string ident = GetIdentifier(p);

	auto it = m_mapConstantDefs.find(ident);
	if (it == m_mapConstantDefs.end())
		throw CParseException("symbol " + ident + " undefined", m_nCurrentLine);

	return it->second;
}


// ===============================================================================
//							CAutoVersion::ParseConstantDef
// ===============================================================================
void CAutoVersion::ParseConstantDef(char *&p)
{
	string ident = GetIdentifier(p);
	string what = GetLiteralOrSymbol(p);

	auto it = m_mapConstantDefs.find(ident);
	if (it != m_mapConstantDefs.end())
		throw CParseException("duplicate symbol " + ident, m_nCurrentLine);

	pair<string, string> cd(ident, what);
	m_mapConstantDefs.insert(cd);
}


// ===============================================================================
//							CAutoVersion::ParseReplacement
// ===============================================================================
void CAutoVersion::ParseReplacement(EReplaceOp op, char *&p)
{
	size_t offset;
	string file_name = GetLiteral(p);		// file name
	string what = GetLiteral(p, &offset);	// what to replace

	SkipWhiteSpaces(p);
	if (*p != '@')
		throw CParseException("@ symbol missing", m_nCurrentLine);

	string ident = GetIdentifier(++p);		// get replace with (this is a constant name)

	// get the value of the constant
	string with;
	auto it = m_mapConstantDefs.find(ident);
	if (it == m_mapConstantDefs.end())
		throw CParseException("constant '" + ident + "' not found", m_nCurrentLine);

	with = it->second;
	if (op == enRoBinary && what.length() != with.length())
		throw CParseException("for binary replacements the length of the find string must be equal to the length of the replace string", m_nCurrentLine);

	CReplace replace(op, what, with, offset);

	auto itf = m_mapFiles.find(file_name);
	if (itf != m_mapFiles.end())
	{
		itf->second.Add(replace);
	}
	else
	{
		CFileNode node;
		node.Add(replace);
		pair<string, CFileNode> mf(file_name, node);
		m_mapFiles.insert(mf);
	}
}


// ===============================================================================
//							CAutoVersion::ParseMessage
// ===============================================================================
void CAutoVersion::ParseMessage(char *&p)
{
	const int MaxIdentLen = 2048;
	char ident[MaxIdentLen + 1];

	int i = 0;
	while (*p && *p != '\012' && *p != '\015')
	{
		ident[i++] = *p++;
		if (i >= MaxIdentLen)
			throw CParseException("message too long", m_nCurrentLine);
	}

	ident[i] = '\0';

	m_listMessages.push_back(ident);
}


// ===============================================================================
//							CAutoVersion::ParseCommand
// ===============================================================================
void CAutoVersion::ParseCommand(char *&p)
{
	string ident = GetIdentifier(p);

	if (ident == "Basepath")
	{
		if (!m_strBasePath.empty())
			throw CParseException("basepath already defined", m_nCurrentLine);

		m_strBasePath = GetLiteral(p);
	}
	else if (ident == "if")
	{
		ident = GetIdentifier(p);

		auto it = m_setDefines.find(ident);
		if (it == m_setDefines.end())
		{
			// condition failed, search matching %else or %end token
			int if_count = 1;

			while (if_count > 0)
			{
				// search next command token
				Scan(p, "%");
				if (!p)
					throw CParseException("missing %end token for if-token", m_nCurrentLine);

				if (strncmp(p, "%if", 3) == 0)
				{
					if_count++;
					p += 3;
				}
				else if (strncmp(p, "%else", 5) == 0)
				{
					if (if_count == 1)
						if_count--;
					p += 5;
				}
				else if (strncmp(p, "%end", 4) == 0)
				{
					if_count--;
					p += 4;
				}
				else
					p++;
			}
		}
	}
	else if (ident == "else")
	{
		// skip until matching %end token
		int if_count = 1;

		while (if_count > 0)
		{
			// search next command token
			Scan(p, "%");
			if (!p)
				throw CParseException("missing %end token for if-token", m_nCurrentLine);

			if (strncmp(p, "%if", 3) == 0)
			{
				if_count++;
				p += 3;
			}
			else if (strncmp(p, "%end", 4) == 0)
			{
				if_count--;
				p += 4;
			}
			else
				p++;
		}
	}
	else if (ident == "end")
	{
		// do nothing, just overread
	}
	else if (ident == "shell")
	{
		// get the shell command-string
		string arg = GetLiteral(p);

		// Create the delayed command
		CCommandShell cmd;
		cmd.AddArg(arg);
		m_listDelayedCommands.push_back(cmd);
	}
	else
		throw CParseException("unkown %-command", m_nCurrentLine);
}


// ===============================================================================
//							CAutoVersion::ParseControlFile
// ===============================================================================
void CAutoVersion::ParseControlFile()
{
	struct stat st;
	if (stat(m_strControlFile.c_str(), &st) != 0)
		throw CException("stat failed for file " + m_strControlFile);

	m_pBuffer = (char *)malloc(st.st_size + 1);
	if (!m_pBuffer)
		throw CException("out of memory");

	FILE *fh = fopen(m_strControlFile.c_str(), "rb");	// binary mode is important! otherwise \015 is eaten on read and therefore
	size_t ret = fread(m_pBuffer, 1, st.st_size, fh);	// the computed offsets into the Control File are wrong, so the updating
	m_pBuffer[ret] = '\0';								// the Control File later would fail!
	fclose(fh);

	char *p = m_pBuffer;
	while (*p)
	{
		SkipWhiteSpaces(p);
		char c = *p;

		if (c != '\012' && c != '\015')
		{
			p++;
			if (c == '@')
				ParseConstantDef(p);
			else if (c == '&')
				ParseReplacement(enRoText, p);
			else if (c == '$')
				ParseReplacement(enRoBinary, p);
			else if (c == '!')
				ParseMessage(p);
			else if (c == '%')
				ParseCommand(p);
			else if (c == '#')
				SkipComment(p);
			else
				throw CParseException("unknown command in Control File", m_nCurrentLine);
		}

		SkipLine(p);
	}

	// do not free(m_pBuffer), because we will use it to update the Control File
}


// ===============================================================================
//								CAutoVersion::UpdateControlFile
// ===============================================================================
void CAutoVersion::UpdateControlFile()
{
	if (g_bVerbose)
		printf("\nupdating Control File %s... ", m_strControlFile.c_str());

	// Datei in den Speicher lesen
	struct stat st;
	if (stat(m_strControlFile.c_str(), &st) != 0)
		throw CException("stat failed for file " + m_strControlFile);

	size_t size = st.st_size;
	char *buf = (char *)malloc(size);
	if (!buf)
		throw CException("out of memory");

	FILE *fh = fopen(m_strControlFile.c_str(), "rb");
	size_t ret = fread(buf, 1, size, fh);
	fclose(fh);
	if (ret != size)
		throw CException("reading file " + m_strControlFile + " failed!");

	// Wir müssen die Replacements aufsteigend nach CReplace::m_nControlFilePos sortieren, da
	// durch Ersetzungen kürzere oder längere Strings entstehen können und wir mit einem Korrektur-Offset arbeiten.

	// Für jede Datei:
	vector<CReplace> replacements;
	for (auto it : m_mapFiles)
	{
		// Alle Replacements auf das Control File anwenden
		list<CReplace> &listReplacements = it.second.GetReplacements();
		for (auto it : listReplacements)
			replacements.push_back(it);
	}

	sort(replacements.begin(), replacements.end(), 
		[](CReplace const &a, CReplace const &b) { return a.m_nControlFilePos < b.m_nControlFilePos; });

	int offset = 0;
	for (auto it : replacements)
		buf = it.UpdateControlFile(buf, size, offset);

	Backup(m_strControlFile);
	m_bControlFileSaved	= true;

	// Datei schreiben
	fh = fopen(m_strControlFile.c_str(), "wb");
	if (fwrite(buf, 1, size, fh) != size)
		throw CException("writing file " + m_strControlFile + " failed!");
	fclose(fh);

	free(buf);
	if (g_bVerbose)
		printf("done.\n");
}


// ===============================================================================
//								CAutoVersion::Replace
// ===============================================================================
void CAutoVersion::Replace()
{
	printf("\nscanning for replacement actions...\n");
	ParseControlFile();
	// Dump();

	int  count = 0;
	bool has_replacements = false;

	// Für jede Datei:
	for (auto &it : m_mapFiles)
	{
		// Auf Replacements prüfen
		string fname = m_strBasePath + "\\" + it.first;		// file name
		if (it.second.CheckReplacements(fname))
		{
			count++;
			has_replacements = true;
		}
	}

	printf("\nscanning finished. (%d files will have replacements)\n\n", count);

	if (has_replacements)
	{
		if (m_bInteractive)
		{
			printf("perform replacements (y/n)?");
			char c = (char)_getch();
			if (c == 'n')
				return;
			printf("\n");
		}
	}
	else
	{
		printf("nothing to replace\n");
		return;
	}

	// Für jede Datei:
	printf("replacing...\n");
	for (auto &it : m_mapFiles)
	{
		// Replacements durchführen
		string fname = m_strBasePath + "\\" + it.first;		// file name
		it.second.DoReplacments(fname);
	}

	UpdateControlFile();
	printf("replacement finished.\n");
}


// ===============================================================================
//								CAutoVersion::RescueRollback
//
// Wird im Fehlerfall aufgerufen, führt nur Rollback für die während des
// Programmablaufs erzeugten .avbak Dateien durch.
// ===============================================================================
void CAutoVersion::RescueRollback()
{
	printf("\nperforming rescue rollback...\n");

	for (auto it : m_mapFiles)
	{
		// Rollback durchführen
		string fname = m_strBasePath + "\\" + it.first;		// file name
		it.second.Rollback(fname);
	}

	if (m_bControlFileSaved)
		::Rollback(m_strControlFile);

	printf("done.\n");
}


// ===============================================================================
//								CAutoVersion::Rollback
//
// Normale Rollback-Funktion
// ===============================================================================
void CAutoVersion::Rollback()
{
	if (m_bInteractive)
	{
		printf("perform rollback (y/n)?");
		char c = (char)_getch();
		if (c == 'n')
			return;
	}

	printf("\nperforming rollback...\n");
	ParseControlFile();

	// Für jede Datei testen, ob eine .avbak Datei besteht.
	// Falls ja, dann Rollback für diese Datei durchführen.
	for (auto it : m_mapFiles)
	{
		string fname = m_strBasePath + "\\" + it.first;		// file name
		if (CanRollback(fname))
			::Rollback(fname);
	}

	// Zum Schluß das Control File testen
	if (CanRollback(m_strControlFile))
		::Rollback(m_strControlFile);

	printf("done.\n");
}


// ===============================================================================
//								CAutoVersion::Clean
// ===============================================================================
void CAutoVersion::Clean()
{
	if (m_bInteractive)
	{
		printf("remove rollback files (y/n)?");
		char c = (char)_getch();
		if (c == 'n')
			return;
	}

	printf("\nremoving rollback files...\n");
	ParseControlFile();

	// Für jede Datei testen, ob eine .avbak Datei besteht.
	// Falls ja, dann diese Datei löschen.
	for (auto it : m_mapFiles)
	{
		string fname = m_strBasePath + "\\" + it.first;		// file name
		if (CanRollback(fname))
		{
			fname += ".avbak";
			if (g_bVerbose)
				printf("deleting %s\n", fname.c_str());
			_unlink(fname.c_str());
		}
	}

	// Zum Schluß das Control File testen
	if (CanRollback(m_strControlFile))
	{
		string fname = m_strControlFile + ".avbak";
		if (g_bVerbose)
			printf("deleting %s\n", fname.c_str());
		_unlink(fname.c_str());
	}

	printf("done.\n");
}


// ===============================================================================
//										main
// ===============================================================================
int main(int argc, char* argv[])
{
	printf("\nAutoVersion v2.00 - Copyright (c) 2024 T. Radde\n");

	if (argc < 2 || argc > 6 )
	{
		cerr << "Syntax: " << argv[0] << " [-r | -c] [-d<ident>] [-v] [-y] ControlFile"
			 << endl;
		cerr << "        -r: Rollback" << endl;
		cerr << "        -c: Clean (delete backups)" << endl;
		cerr << "        -d: define ident for conditional replace" << endl;
		cerr << "        -v: Verbose" << endl;
		cerr << "        -y: automatically answer all questions with 'yes'" << endl;
		exit(1);
	}

	enum
	{
		REPLACE_OP,
		ROLLBACK_OP,
		CLEAN_OP,
	};

	CAutoVersion AutoVersion;
	AutoVersion.SetControlFile(argv[argc - 1]);
	int operation = REPLACE_OP;
	g_bVerbose = false;

	try
	{
		// Parse command line arguments
		for (int i = 1; i < argc - 1; i++)
		{
			if (argv[i][0] == '-' && strlen(argv[i]) == 2)
			{
				if ( argv[i][1] == 'v' )
					g_bVerbose = true;
				else if ( argv[i][1] == 'r' )
					operation = ROLLBACK_OP;
				else if ( argv[i][1] == 'c' )
					operation = CLEAN_OP;
				else if ( argv[i][1] == 'y' )
					AutoVersion.SetInteractive(false);
				else
				{
					cerr << "Invalid option " << argv[i] << endl;
					exit(1);
				}
			}
			else if (argv[i][0] == '-' && argv[i][1] == 'd' && strlen(argv[i]) > 2)
			{
				AutoVersion.AddDefine(argv[i] + 2);
			}
			else
			{
				cerr << "Invalid option!" << endl;
				exit(1);
			}
		}

		switch (operation)
		{
			case REPLACE_OP:
				AutoVersion.Replace();
				AutoVersion.ExecDelayedCommands();
				AutoVersion.ShowMessages();
				break;

			case ROLLBACK_OP:
				AutoVersion.Rollback();
				AutoVersion.ExecDelayedCommands();
				break;

			case CLEAN_OP:
				AutoVersion.Clean();
				break;

			default:
				throw CException("unknown operation");
				break;
		}
	}
	catch (exception &e)
	{
		printf("\nError: %s\n", e.what());
		AutoVersion.RescueRollback();
		return 1;
	}
	catch (...)
	{
		printf("\nError: unhandled exception\n");
		AutoVersion.RescueRollback();
		return 1;
	}

	return 0;
}
