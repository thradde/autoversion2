/*
* autoversion.h
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

#ifndef _AUTOVERSION_H_
#define _AUTOVERSION_H_

// ========================================================================
//                            Globals
// ========================================================================
extern 	bool g_bVerbose;	// program is verbose


// ========================================================================
//                            ToString
// ========================================================================
template<class T>
string ToString(const T& val)
{
    std::stringstream strm;
    strm << val;
    return strm.str();
}


// ===============================================================================
//									class CException
// ===============================================================================
class CException : public exception
{
protected:
	string m_strMessage;

public:
	CException(const string &message)
	{
		m_strMessage = message;
	}

	virtual ~CException() throw() {}

	virtual const char* what() const throw()
	{
		return m_strMessage.c_str();
	}
};


// ===============================================================================
//									class CParseException
// ===============================================================================
class CParseException : public exception
{
protected:
	string m_strMessage;

public:
	CParseException(const string &message, int line_number)
	{
		m_strMessage = message + " at line " + ToString(line_number);
	}

	virtual ~CParseException() throw() {}

	virtual const char* what() const throw()
	{
		return m_strMessage.c_str();
	}
};


// ===============================================================================
//									class CReplace
//
// A single replacement operation.
// ===============================================================================
enum EReplaceOp
{
	enRoText,		// text replacement
	enRoBinary,		// binary replacement
};


class CReplace
{
protected:
	EReplaceOp	m_enReplaceOp;		// the operation, either text or binary
	string		m_strWhat;			// what to replace
	string		m_strWith;			// to replace with
	bool		m_bMustReplace;		// true if "what" was found
	bool		m_bDidReplace;		// true if replacement was done

public:
	size_t		m_nControlFilePos;	// offset-position (in bytes) within the Control File, where the "what" string is found
									// this is used to update the Control File

public:
	CReplace(EReplaceOp op, const string &what, const string &with, size_t nControlFilePos)
	{
		m_enReplaceOp		= op;
		m_strWhat			= what;
		m_strWith			= with;
		m_nControlFilePos	= nControlFilePos;
		m_bMustReplace		= false;
		m_bDidReplace		= false;
	}

	bool	CheckReplace(const string &file_name, char *buf, size_t size);	// checks, if a replacement will occur
	char	*DoReplace(char *buf, size_t &size);							// performs the replacement
	char	*UpdateControlFile(char *buf, size_t &size, int &offset);		// Alle Replacements auf das Control File anwenden

#ifdef _DEBUG
	void	Dump()		// show parsed structures of Control File
	{
		string op;
		if (m_enReplaceOp == enRoText)
			op = "text";
		else if (m_enReplaceOp == enRoBinary)
			op = "binary";
		else
			throw CException("unknown type of m_enReplaceOp");

		printf("Type: %s --- What: %s --- With: %s --- Must Replace: %s --- Did Replace: %s\n",
			op.c_str(),
			m_strWhat.c_str(),
			m_strWith.c_str(),
			m_bMustReplace ? "yes" : "no",
			m_bDidReplace ? "yes" : "no");
	}
#endif
};


// ===============================================================================
//									class CFileNode
//
// This represents a file where replacements shall be performed.
// All replacement operations for a single file are held in a list here.
// ===============================================================================
class CFileNode
{
protected:
	list<CReplace>	m_listReplacements;		// All replacement operations for a single file are held in a list here.
	bool			m_bMustReplace;			// true if anything must be replaced in this file
	bool			m_bDidReplace;			// true if replacement was done

public:
	CFileNode()
	{
		m_bMustReplace	= false;
		m_bDidReplace	= false;
	}

	list<CReplace>	&GetReplacements() { return m_listReplacements; }

	void	Add(CReplace r)	{ m_listReplacements.push_back(r); }

	bool	CheckReplacements(const string &file_name);					// checks, if any replacement for this file will occur
	void	DoReplacments(const string &file_name);						// performs all replacements for this file
	void	Rollback(const string &file_name);							// performs a rollback for this file

#ifdef _DEBUG
	void	Dump()		// show parsed structures of Control File
	{
		printf("Must Replace %s\n", m_bMustReplace ? "yes" : "no");
		printf("Did Replace %s\n", m_bDidReplace ? "yes" : "no");

		for (auto it : m_listReplacements)
			it.Dump();
	}
#endif
};


// ===============================================================================
//									class CCommand
// ===============================================================================
class CCommand
{
protected:
	list<string>	m_listArgs;		// arguments
	
public:
	void AddArg(const string &s)
	{
		m_listArgs.push_back(s);
	}

	virtual bool Execute() = 0;
};


// ===============================================================================
//									class CCommandShell
// ===============================================================================
class CCommandShell : public CCommand
{
public:
	virtual bool Execute() override
	{
		string cmd = m_listArgs.front();

		if (g_bVerbose)
			printf("shell: %s\n", cmd.c_str());

		int ret = system(cmd.c_str());
		return ret == 0;
	}
};


// ===============================================================================
//									class CAutoVersion
// ===============================================================================
class CAutoVersion
{
protected:
	bool	m_bInteractive;		// program is interactive, if false, all questions are answered by default with yes
	string	m_strControlFile;	// the name of the Control File
	bool	m_bControlFileSaved;// true, if a rollback file for the Control File was created
	string	m_strBasePath;		// the base path, see %Basepath command in Control File
	int		m_nCurrentLine;		// Current Line number while parsing Control File
	char	*m_pBuffer;			// holds the Control File while parsing

	unordered_set<string>				m_setDefines;			// defines through -d switch
	unordered_map<string, string>		m_mapConstantDefs;		// definitions of constants in Control File
	unordered_map<string, CFileNode>	m_mapFiles;				// the files listed in the Control File
	list<string>						m_listMessages;			// messages in the Control File
	list<CCommandShell>					m_listDelayedCommands;	// Commands executed after replacement has done, e.g. "copy"

	void	SkipWhiteSpaces(char *&p);
	void	SkipLine(char *&p, bool only_white_spaces = true);
	void	SkipComment(char *&p);
	void	Scan(char *&p, const char *token);
	string	GetIdentifier(char *&p);
	string	GetLiteral(char *&p, size_t *offset = NULL);
	string	GetLiteralOrSymbol(char *&p);
	void	ParseConstantDef(char *&p);
	void	ParseReplacement(EReplaceOp op, char *&p);
	void	ParseMessage(char *&p);
	void	ParseCommand(char *&p);
	void	UpdateControlFile();

public:
	CAutoVersion()
	{
		m_bInteractive		= true;
		m_bControlFileSaved	= false;
		m_nCurrentLine		= 1;
		m_pBuffer			= NULL;
	}

	bool	GetInteractive() const { return m_bInteractive; }
	void	SetInteractive(bool val) { m_bInteractive = val; }

	void	AddDefine(const string &d) { m_setDefines.insert(d); }

	const	string	&GetControlFile() const { return m_strControlFile; }
	void			SetControlFile(const string &val) { m_strControlFile = val; }

	int		GetCurrentLine() const { return m_nCurrentLine; }

	void	ParseControlFile();
	void	Replace();
	void	RescueRollback();
	void	Rollback();
	void	Clean();

	void ShowMessages()
	{
		printf("\n\n");
		for (auto it : m_listMessages)
			printf("%s\n", it.c_str());
	}

	void ExecDelayedCommands()
	{
		if (g_bVerbose && m_listDelayedCommands.size() > 0)
			printf("\nexecuting delayed commands\n");

		for (auto it : m_listDelayedCommands)
		{
			if (!it.Execute())
				throw CException("a delayed command failed");
		}
	}

#ifdef _DEBUG
	void Dump()		// show parsed structures of Control File
	{
		printf("Verbose %s\n", g_bVerbose ? "yes" : "no");
		printf("Interactive %s\n", m_bInteractive ? "yes" : "no");
		printf("Control File %s\n", m_strControlFile.c_str());
		printf("Base Path %s\n", m_strBasePath.c_str());

		printf("\nCommand-Line Defines:\n");
		for (auto it : m_setDefines)
			printf("%s\n", it.c_str());

		printf("\nConstants:\n");
		for (auto it : m_mapConstantDefs)
			printf("%s\t\t%s\n", it.first.c_str(), it.second.c_str());

		printf("\nReplacement-Definitions:\n");
		for (auto it : m_mapFiles)
		{
			printf("\nFile: %s\n", it.first.c_str());
			it.second.Dump();
		}

		printf("\nMessages:\n");
		for (auto it : m_listMessages)
			printf("%s\n", it.c_str());
	}
#endif
};

#endif	// _AUTOVERSION_H_
