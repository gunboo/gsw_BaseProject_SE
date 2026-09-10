#pragma once

#include <map>
#include <string>
#include <vector>

struct DialogueLine
{
	std::wstring speaker;		// empty for narration and UI strings
	std::wstring text;
};

struct DialogueBlock
{
	std::vector<DialogueLine> lines;
};

// Reads Data/dialogue.txt. Keeping every Korean string out of the source means
// the .cpp files stay plain ASCII, which side-steps MSVC reading them in the
// system codepage, and lets the writing be edited without a rebuild.
class DialogueDB
{
public:
	bool Load(const char* path);

	const DialogueBlock* Find(const std::string& key) const;

	// Convenience for single-line UI strings; returns an empty string if absent.
	std::wstring Line(const std::string& key, size_t index = 0) const;

private:
	std::map<std::string, DialogueBlock> m_Blocks;
};
