#pragma once

class Ident
{
public:
	~Ident();

	char		*	mString;
	unsigned int	mHash;

	static const Ident* Unique(const char* str);
protected:
	Ident(const char* str, unsigned int hash);
};

class IdentDict
{
public:
	IdentDict(void);
	~IdentDict(void);

	void Insert(const Ident* ident, const char * str);
	const char * Lookup(const Ident* ident);

protected:
	void InsertCopy(const Ident* ident, char* str);

	struct Entry
	{
		const Ident* mIdent;
		char* mString;
	};
	Entry* mHash;
	int			mHashSize, mHashFill;
};