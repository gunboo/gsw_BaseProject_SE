#include "stdafx.h"
#include "Dialogue.h"
#include "TextRenderer.h"

#include <fstream>
#include <iostream>

bool DialogueDB::Load(const char* path)
{
	std::ifstream file(path, std::ios::binary);
	if (file.fail())
	{
		std::cout << path << " could not be opened.\n";
		return false;
	}

	std::string key;
	std::wstring speaker;

	std::string line;
	while (std::getline(file, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
		{
			line.erase(line.size() - 1);
		}

		if (line.empty() || line[0] == '#')
		{
			continue;
		}

		if (line[0] == '@')
		{
			key = line.substr(1);
			speaker.clear();
			m_Blocks[key] = DialogueBlock();
			continue;
		}

		if (key.empty())
		{
			continue;
		}

		if (line[0] == '$')
		{
			speaker = Utf8ToWide(line.substr(1));
			continue;
		}

		DialogueLine entry;
		entry.speaker = speaker;
		entry.text = Utf8ToWide(line);
		m_Blocks[key].lines.push_back(entry);
	}

	std::cout << path << " loaded: " << m_Blocks.size() << " blocks\n";
	return !m_Blocks.empty();
}

const DialogueBlock* DialogueDB::Find(const std::string& key) const
{
	std::map<std::string, DialogueBlock>::const_iterator it = m_Blocks.find(key);
	if (it == m_Blocks.end())
	{
		return NULL;
	}
	return &it->second;
}

std::wstring DialogueDB::Line(const std::string& key, size_t index) const
{
	const DialogueBlock* block = Find(key);
	if (block == NULL || index >= block->lines.size())
	{
		return std::wstring();
	}
	return block->lines[index].text;
}
