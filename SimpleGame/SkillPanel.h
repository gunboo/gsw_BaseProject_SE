#pragma once

#include "Dialogue.h"
#include "MapView.h"
#include "SkillCombat.h"
#include "Renderer.h"

class SkillPanel
{
public:
	void Initialize(PlayerStats& stats, SkillCombat& combat, DialogueDB& dialogue);
	bool IsOpen() const { return m_Open; }
	bool OnKey(unsigned char key, bool down, bool canOpen);
	void OnPointer(float x, float y, bool down, const Renderer& renderer);
	void CancelPointer() { m_MouseDown = false; }
	void Draw(Renderer& renderer);
	void DrawHotbar(Renderer& renderer, bool bow);
	void NotifyCast(int skill, bool succeeded);

private:
	void Learn();
	void Advance();
	std::wstring Text(const std::string& key) const;
	std::wstring Requirement(int skill) const;
	std::wstring ValueText(int skill, int rank) const;
	PlayerStats* m_Stats = NULL;
	SkillCombat* m_Combat = NULL;
	DialogueDB* m_Dialogue = NULL;
	bool m_Open = false;
	bool m_MouseDown = false;
	int m_Tab = 1;
	int m_Selected = 0;
	std::wstring m_Feedback;
	std::wstring m_CastFeedback;
	double m_FeedbackUntil = 0.0;
};
