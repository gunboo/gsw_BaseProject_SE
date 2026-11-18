#include "stdafx.h"
#include "SkillPanel.h"
#include "AnalysisLog.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace
{
	const float WIDTH = 800.0f;
	const float HEIGHT = 536.0f;
	const float DEPTH = 970000.0f;
	const Color INK = { 0.16f, 0.22f, 0.29f, 1 };
	const Color BLUE = { 0.19f, 0.53f, 0.74f, 1 };
	const Color ORANGE = { 0.99f, 0.62f, 0.15f, 1 };
	const Color WHITE = { 1, 1, 1, 1 };
	const Color MUTED = { 0.43f, 0.49f, 0.55f, 1 };
	const Color ICONS[6] = {
		{0.83f, 0.34f, 0.22f, 1}, {0.22f, 0.57f, 0.81f, 1}, {0.40f, 0.51f, 0.87f, 1},
		{0.86f, 0.60f, 0.20f, 1}, {0.27f, 0.64f, 0.49f, 1}, {0.72f, 0.36f, 0.63f, 1}
	};
	const int ACTIVE_SKILLS[] = { SKILL_COMBO, SKILL_BREAKER, SKILL_WARD, SKILL_BARRAGE, SKILL_SHADOW, SKILL_STORM };

	float PanelScale(const Renderer& renderer)
	{
		return std::max(0.1f, std::min(1.0f, std::min((renderer.GetWidth() - 24.0f) / WIDTH,
			(renderer.GetHeight() - 24.0f) / HEIGHT)));
	}

	struct Canvas
	{
		Renderer& renderer;
		float scale;
		float X(float x) const { return (x - WIDTH * 0.5f) * scale; }
		float Y(float y) const { return (y - HEIGHT * 0.5f) * scale; }
		void Rect(float x, float y, float w, float h, const Color& color, float layer = 0) const
		{
			renderer.PushRect(X(x), Y(y), w * scale, h * scale, color, DEPTH + layer);
		}
		void Text(const std::wstring& text, float x, float y, int size, const Color& color, bool bold = false) const
		{
			renderer.PushText(text, X(x), Y(y), std::max(1, static_cast<int>(size * scale)), FONT_UI, bold, color);
		}
		float Wrap(const std::wstring& text, float x, float y, float width, int size, const Color& color) const
		{
			std::wstring line;
			const int font = std::max(1, static_cast<int>(size * scale));

			for (wchar_t character : text)
			{
				if (character == L'\n' || (!line.empty()
					&& renderer.MeasureTextWidth(line + character, font, FONT_UI, false) > width * scale))
				{
					Text(line, x, y, size, color);
					line.clear();
					y += size + 6.0f;
				}

				if (character != L'\n')
				{
					line += character;
				}
			}

			Text(line, x, y, size, color);
			return y + size + 6.0f;
		}
	};

	void Icon(Renderer& renderer, int skill, float x, float y, float size, bool enabled, float depth)
	{
		const Color background = enabled ? ICONS[skill % 6] : RGBA(0.49f, 0.53f, 0.58f);
		renderer.PushRect(x, y, size, size, RGBA(0.14f, 0.22f, 0.32f), depth);
		renderer.PushRect(x + 2, y + 2, size - 4, size - 4, background, depth + 1);
		const float centerX = x + size * 0.5f, centerY = y + size * 0.5f;
		const float unit = size * 0.22f;
		const Color light = enabled ? WHITE : RGBA(0.72f, 0.75f, 0.78f);

		if (skill == SKILL_WARD || skill == SKILL_VITALITY || skill == SKILL_LEECH)
		{
			renderer.PushDiamond(centerX, centerY, unit, unit * 1.3f, light, depth + 2);
			renderer.PushRect(centerX - unit * 0.18f, centerY - unit * 0.65f, unit * 0.36f, unit * 1.3f, background, depth + 3);
			renderer.PushRect(centerX - unit * 0.65f, centerY - unit * 0.18f, unit * 1.3f, unit * 0.36f, background, depth + 3);
		}
		else if (skill == SKILL_FLEET || skill == SKILL_SHADOW || skill == SKILL_FOCUS)
		{
			for (int i = -1; i <= 1; ++i)
			{
				const float offset = i * unit * 0.65f;
				renderer.PushLine(centerX - unit, centerY + offset + unit * 0.5f,
					centerX + unit, centerY + offset - unit * 0.5f, size * 0.08f, light, depth + 2);
			}
		}
		else
		{
			renderer.PushLine(centerX - unit, centerY + unit, centerX + unit, centerY - unit, size * 0.13f, light, depth + 2);
			renderer.PushLine(centerX - unit, centerY - unit, centerX + unit, centerY + unit, size * 0.07f, light, depth + 2);
			renderer.PushDiamond(centerX + unit, centerY + unit, size * 0.10f, size * 0.10f, light, depth + 2);
		}
	}
}

void SkillPanel::Initialize(PlayerStats& stats, SkillCombat& combat, DialogueDB& dialogue)
{
	m_Stats = &stats;
	m_Combat = &combat;
	m_Dialogue = &dialogue;
	m_Open = m_MouseDown = false;
	m_Tab = 1;
	m_Selected = 0;
	m_Feedback.clear();
	m_CastFeedback.clear();
	m_FeedbackUntil = 0.0;
	AnalysisLog::Get().Event("skill.ready", "skills=12;tier=" + std::to_string(stats.skills.Tier())
		+ ";sp=" + std::to_string(stats.skills.Points()));
}

std::wstring SkillPanel::Text(const std::string& key) const
{
	const DialogueBlock* block = m_Dialogue->Find(key);
	std::wstring result;

	if (block != NULL)
	{
		for (const DialogueLine& line : block->lines)
		{
			if (!result.empty())
			{
				result += L'\n';
			}

			result += line.text;
		}
	}

	return result;
}

std::wstring SkillPanel::Requirement(int skill) const
{
	const SkillBook& book = m_Stats->skills;
	const SkillDefinition& definition = GetSkillDefinition(skill);

	if (definition.tier > book.Tier())
	{
		return Text("ui_skill_need_job");
	}

	if (definition.prerequisite >= 0 && book.Rank(definition.prerequisite) < definition.requiredRank)
	{
		return Text("ui_skill_need") + L" " + Text(GetSkillDefinition(definition.prerequisite).textKey)
			+ L" Lv." + std::to_wstring(definition.requiredRank);
	}

	if (book.Rank(skill) == SkillBook::MAX_RANK)
	{
		return Text("ui_skill_mastered");
	}

	return book.Points() > 0 ? Text("ui_skill_can_learn") : Text("ui_skill_no_sp");
}

std::wstring SkillPanel::ValueText(int skill, int rank) const
{
	const float value = GetSkillDefinition(skill).Value(rank);
	const bool health = skill == SKILL_VITALITY;
	const bool time = skill == SKILL_SHADOW;
	wchar_t number[32];
	std::swprintf(number, 32, time ? L"%.2f s" : health ? L"%.0f HP" : L"%.0f%%", time || health ? value : value * 100.0f);
	return Text(std::string(GetSkillDefinition(skill).textKey) + "_value") + L" " + number;
}

void SkillPanel::Learn()
{
	if (m_Stats->LearnSkill(m_Selected))
	{
		m_Feedback = Text("ui_skill_learned") + L" " + Text(GetSkillDefinition(m_Selected).textKey);
		AnalysisLog::Get().Event("skill.learn", std::string(GetSkillDefinition(m_Selected).textKey)
			+ ";rank=" + std::to_string(m_Stats->skills.Rank(m_Selected)));
	}
	else
	{
		m_Feedback = Requirement(m_Selected);
	}
}

void SkillPanel::Advance()
{
	if (m_Stats->skills.Advance())
	{
		m_Tab = 2;
		m_Selected = 6;
		m_Feedback = Text("ui_skill_advanced");
		AnalysisLog::Get().Event("skill.advance", "tier=2");
	}
	else
	{
		m_Feedback = Text(m_Stats->skills.Tier() == 2 ? "ui_skill_advanced" : "ui_skill_job_requirement");
	}
}

bool SkillPanel::OnKey(unsigned char key, bool down, bool canOpen)
{
	if (m_Stats == NULL)
	{
		return false;
	}

	if (down && ((key == 'k' && (canOpen || m_Open)) || (key == 27 && m_Open)))
	{
		m_Open = !m_Open;
		m_MouseDown = false;
		m_Feedback.clear();
		AnalysisLog::Get().Event("skill.panel", m_Open ? "open;paused" : "closed");
		return true;
	}

	if (!m_Open)
	{
		return false;
	}

	if (down)
	{
		if (key == '1' || key == '2')
		{
			m_Tab = key - '0';
			m_Selected = (m_Tab - 1) * 6;
		}
		else if (key == 'w' || key == 's')
		{
			m_Selected = (m_Tab - 1) * 6 + (m_Selected % 6 + (key == 'w' ? 5 : 1)) % 6;
		}
		else if (key == '\r')
		{
			Learn();
		}
		else if (key == 'p')
		{
			Advance();
		}
	}

	return true;
}

void SkillPanel::OnPointer(float x, float y, bool down, const Renderer& renderer)
{
	const bool clicked = down && !m_MouseDown;
	m_MouseDown = down;

	if (!m_Open || !clicked)
	{
		return;
	}

	const float scale = PanelScale(renderer);
	x = x / scale + WIDTH * 0.5f;
	y = y / scale + HEIGHT * 0.5f;

	if (MapRect{ 756, 10, 30, 24 }.Contains(x, y))
	{
		m_Open = false;
		return;
	}

	for (int tab = 1; tab <= 2; ++tab)
	{
		if (MapRect{ 20.0f + (tab - 1) * 206.0f, 52, 198, 34 }.Contains(x, y))
		{
			m_Tab = tab;
			m_Selected = (tab - 1) * 6;
			m_Feedback.clear();
			return;
		}
	}

	for (int row = 0; row < 6; ++row)
	{
		if (MapRect{ 24, 126.0f + row * 51.0f, 410, 47 }.Contains(x, y))
		{
			m_Selected = (m_Tab - 1) * 6 + row;
			m_Feedback.clear();

			if (x >= 398)
			{
				Learn();
			}

			return;
		}
	}

	if (MapRect{ 470, 412, 298, 32 }.Contains(x, y))
	{
		Learn();
	}
	else if (MapRect{ 554, 476, 214, 34 }.Contains(x, y))
	{
		Advance();
	}
}

void SkillPanel::Draw(Renderer& renderer)
{
	Canvas canvas = { renderer, PanelScale(renderer) };
	const SkillBook& book = m_Stats->skills;
	renderer.ClearAnim();
	canvas.Rect(4, 5, WIDTH, HEIGHT, RGBA(0, 0, 0, 0.35f));
	canvas.Rect(0, 0, WIDTH, HEIGHT, BLUE, 1);
	canvas.Rect(3, 3, WIDTH - 6, HEIGHT - 6, RGBA(0.89f, 0.95f, 0.98f), 2);
	canvas.Rect(3, 3, WIDTH - 6, 39, ORANGE, 3);
	canvas.Text(L"SKILL", 18, 12, 20, WHITE, true);
	canvas.Text(Text("ui_skill_title"), 98, 16, 14, INK, true);
	canvas.Rect(756, 10, 30, 24, RGBA(0.79f, 0.34f, 0.12f), 4);
	canvas.Text(L"X", 766, 12, 17, WHITE, true);

	for (int tab = 1; tab <= 2; ++tab)
	{
		const float x = 20.0f + (tab - 1) * 206.0f;
		canvas.Rect(x, 52, 198, 34, tab == m_Tab ? BLUE : RGBA(0.70f, 0.81f, 0.88f), 4);
		canvas.Text(Text(tab == 1 ? "ui_skill_job1" : "ui_skill_job2"), x + 13, 61, 15,
			tab == m_Tab ? WHITE : INK, true);
	}

	canvas.Text(L"Lv." + std::to_wstring(m_Stats->level) + L"   SP " + std::to_wstring(book.Points()), 470, 58, 19, BLUE, true);
	canvas.Text(Text("ui_skill_list_hint"), 26, 102, 12, MUTED);
	canvas.Rect(20, 121, 418, 310, RGBA(0.67f, 0.79f, 0.87f), 3);

	for (int row = 0; row < 6; ++row)
	{
		const int id = (m_Tab - 1) * 6 + row;
		const SkillDefinition& definition = GetSkillDefinition(id);
		const float y = 126.0f + row * 51.0f;
		canvas.Rect(24, y, 410, 47, m_Selected == id ? RGBA(1, 0.94f, 0.76f) : WHITE, 4);
		Icon(renderer, id, canvas.X(30), canvas.Y(y + 4), 39 * canvas.scale, book.Unlocked(id), DEPTH + 5);
		canvas.Text(Text(definition.textKey), 80, y + 5, 15, INK, true);
		const std::wstring kind = definition.Active() ? L"[" + std::wstring(1, static_cast<wchar_t>(definition.key - 'a' + 'A'))
			+ L"] " + Text("ui_skill_active") : Text("ui_skill_passive");
		canvas.Text(kind, 80, y + 26, 11, MUTED);
		canvas.Text(std::to_wstring(book.Rank(id)) + L" / 5", 337, y + 14, 14, BLUE, true);
		canvas.Rect(398, y + 9, 28, 28, book.CanLearn(id) ? ORANGE : RGBA(0.78f, 0.81f, 0.83f), 5);
		canvas.Text(L"+", 405, y + 9, 23, WHITE, true);
	}

	canvas.Rect(452, 94, 330, 358, WHITE, 3);
	const SkillDefinition& selected = GetSkillDefinition(m_Selected);
	Icon(renderer, m_Selected, canvas.X(470), canvas.Y(110), 48 * canvas.scale, book.Unlocked(m_Selected), DEPTH + 5);
	canvas.Text(Text(selected.textKey), 530, 114, 20, INK, true);
	canvas.Text(L"Lv." + std::to_wstring(book.Rank(m_Selected)) + L" / 5", 530, 142, 13, BLUE, true);
	float y = canvas.Wrap(Text(std::string(selected.textKey) + "_desc"), 470, 178, 298, 12, INK) + 8;
	canvas.Text(Text("ui_skill_current"), 470, y, 12, BLUE, true);
	y = canvas.Wrap(ValueText(m_Selected, book.Rank(m_Selected)), 470, y + 20, 298, 13, INK) + 5;

	if (book.Rank(m_Selected) < SkillBook::MAX_RANK)
	{
		canvas.Text(Text("ui_skill_next"), 470, y, 12, BLUE, true);
		y = canvas.Wrap(ValueText(m_Selected, book.Rank(m_Selected) + 1), 470, y + 20, 298, 13, INK) + 5;
	}

	if (selected.Active())
	{
		wchar_t cooldown[32];
		std::swprintf(cooldown, 32, L" %.1f s", book.Cooldown(m_Selected));
		canvas.Text(Text("ui_skill_cooldown") + cooldown, 470, y, 12, MUTED);
	}

	canvas.Wrap(Requirement(m_Selected), 470, 374, 298, 12, BLUE);
	canvas.Rect(470, 412, 298, 32, book.CanLearn(m_Selected) ? ORANGE : RGBA(0.68f, 0.74f, 0.78f), 4);
	canvas.Text(Text("ui_skill_spend"), 548, 420, 14, WHITE, true);
	canvas.Text(m_Feedback, 26, 447, 12, BLUE, true);
	canvas.Rect(20, 475, 518, 38, RGBA(0.80f, 0.89f, 0.94f), 3);
	canvas.Wrap(Text(book.Tier() == 1 ? "ui_skill_job_requirement" : "ui_skill_advanced"), 30, 482, 498, 12, INK);
	canvas.Rect(554, 476, 214, 34, book.CanAdvance() ? ORANGE : BLUE, 4);
	canvas.Text(Text(book.Tier() == 1 ? "ui_skill_advance" : "ui_skill_job2"), 575, 485, 13, WHITE, true);
	canvas.Text(Text("ui_skill_keyboard"), 24, 518, 10, MUTED);
}

void SkillPanel::NotifyCast(int skill, bool succeeded)
{
	if (succeeded)
	{
		m_CastFeedback = Text(GetSkillDefinition(skill).textKey);
	}
	else if (m_Stats->skills.Rank(skill) == 0)
	{
		m_CastFeedback = Text("ui_skill_not_learned");
	}
	else
	{
		m_CastFeedback = Text("ui_skill_wait");
	}

	m_FeedbackUntil = AnalysisLog::NowMs() * 0.001 + 1.8;
}

void SkillPanel::DrawHotbar(Renderer& renderer, bool bow)
{
	const float size = std::max(12.0f, std::min(42.0f, (renderer.GetWidth() - 40.0f) / 7.0f));
	const float left = -(size + 6.0f) * 3.0f;
	const float top = renderer.GetHeight() * 0.5f - 144.0f;
	renderer.DiscardTextInRect(left - 8, top - 30, (size + 6) * 6 + 16, size + 50);
	renderer.PushRect(left - 8, top - 30, (size + 6) * 6 + 16, size + 50, RGBA(0.04f, 0.08f, 0.12f, 0.92f), DEPTH);
	renderer.PushText(Text(bow ? "ui_skill_bar_bow" : "ui_skill_bar_sword") + L"   [K] SP "
		+ std::to_wstring(m_Stats->skills.Points()), left, top - 23, 11, FONT_UI, true, WHITE);

	for (int i = 0; i < 6; ++i)
	{
		const int id = ACTIVE_SKILLS[i];
		const float x = left + i * (size + 6);
		const bool learned = m_Stats->skills.Rank(id) > 0;
		Icon(renderer, id, x, top, size, learned, DEPTH + 1);
		const float cooldown = m_Combat->Remaining(id);

		if (cooldown > 0.0f)
		{
			const float fraction = std::min(1.0f, cooldown / std::max(0.01f, m_Stats->skills.Cooldown(id)));
			renderer.PushRect(x, top, size, size * fraction, RGBA(0, 0, 0, 0.65f), DEPTH + 6);
			renderer.PushText(std::to_wstring(static_cast<int>(std::ceil(cooldown))), x + size * 0.5f, top + size * 0.3f,
				14, FONT_UI, true, WHITE, ALIGN_CENTER);
		}

		const wchar_t key = static_cast<wchar_t>(GetSkillDefinition(id).key - 'a' + 'A');
		renderer.PushText(std::wstring(1, key), x + 3, top + 1, 11, FONT_UI, true, WHITE);
		renderer.PushText(learned ? std::to_wstring(m_Stats->skills.Rank(id)) : L"-", x + size - 4, top + size - 13,
			10, FONT_UI, true, WHITE, ALIGN_RIGHT);
	}

	if (AnalysisLog::NowMs() * 0.001 < m_FeedbackUntil)
	{
		renderer.PushText(m_CastFeedback, 0, top - 50, 13, FONT_UI, true, RGBA(1, 0.84f, 0.4f), ALIGN_CENTER);
	}
	else if (m_Combat->WardRemaining() > 0)
	{
		renderer.PushText(Text("skill_ward") + L" " + std::to_wstring(static_cast<int>(std::ceil(m_Combat->WardRemaining()))) + L"s",
			0, top - 50, 13, FONT_UI, true, RGBA(0.5f, 0.85f, 1), ALIGN_CENTER);
	}
}
