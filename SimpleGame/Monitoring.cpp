#include "stdafx.h"
#include "Monitoring.h"
#include "AnalysisLog.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Platform.h"
#ifdef _WIN32
#include <Windows.h>
#include <winhttp.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#endif
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <locale>
#include <sstream>

#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif

namespace
{
	const double PUBLISH_INTERVAL_MS = 1000.0;
	const int HTTP_TIMEOUT_MS = 250;
	const char* const HEX = "0123456789abcdef";

	std::string Environment(const char* name)
	{
		return Platform::Environment(name);
	}

#ifndef _WIN32
	bool PostSnapshot(unsigned short port, const std::string& message)
	{
		const int connection = socket(AF_INET, SOCK_STREAM, 0);

		if (connection < 0)
		{
			return false;
		}

		timeval timeout = { 0, HTTP_TIMEOUT_MS * 1000 };
		setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		setsockopt(connection, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#ifdef __APPLE__
		const int noSignal = 1;
		setsockopt(connection, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
#endif
		const int flags = fcntl(connection, F_GETFL, 0);
		fcntl(connection, F_SETFL, flags | O_NONBLOCK);
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		bool connected = connect(connection, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;

		if (!connected && errno == EINPROGRESS)
		{
			pollfd pending = { connection, POLLOUT, 0 };
			int error = 0;
			socklen_t length = sizeof(error);
			connected = poll(&pending, 1, HTTP_TIMEOUT_MS) > 0
				&& getsockopt(connection, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0;
		}

		fcntl(connection, F_SETFL, flags);
		const std::string request = "POST /api/snapshot HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n"
			"Content-Type: application/json; charset=utf-8\r\nContent-Length: " + std::to_string(message.size()) + "\r\n\r\n" + message;
		size_t sent = 0;

		while (connected && sent < request.size())
		{
#ifdef MSG_NOSIGNAL
			const int sendFlags = MSG_NOSIGNAL;
#else
			const int sendFlags = 0;
#endif
			const ssize_t bytes = send(connection, request.data() + sent, request.size() - sent, sendFlags);

			if (bytes <= 0)
			{
				connected = false;
				break;
			}

			sent += static_cast<size_t>(bytes);
		}

		std::string response;
		char buffer[256];

		while (connected && response.find("\r\n") == std::string::npos && response.size() < 1024)
		{
			const ssize_t bytes = recv(connection, buffer, sizeof(buffer), 0);

			if (bytes <= 0)
			{
				break;
			}

			response.append(buffer, static_cast<size_t>(bytes));
		}

		close(connection);
		int status = 0;
		std::istringstream line(response);
		std::string protocol;
		line >> protocol >> status;

		return connected && protocol.compare(0, 5, "HTTP/") == 0 && status >= 200 && status < 300;
	}
#endif

	bool IsIdentifier(const std::string& value)
	{
		if (value.empty() || value.size() > 128)
		{
			return false;
		}

		for (const unsigned char c : value)
		{
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
				|| (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == ':'))
			{
				return false;
			}
		}

		return true;
	}

	void String(std::ostream& out, const std::string& value)
	{
		out << '"';

		for (const unsigned char c : value)
		{
			if (c == '"' || c == '\\')
			{
				out << '\\' << static_cast<char>(c);
			}
			else if (c < 0x20)
			{
				out << "\\u00" << HEX[c >> 4] << HEX[c & 15];
			}
			else
			{
				out << static_cast<char>(c);
			}
		}

		out << '"';
	}

	void Number(std::ostream& out, float value)
	{
		// A bad simulation value must not produce invalid JSON (NaN/Infinity).
		out << (std::isfinite(value) ? value : 0.0f);
	}

	void Entities(std::ostream& out, const std::vector<MonitoredEntity>& entities)
	{
		out << '[';

		for (size_t i = 0; i < entities.size(); ++i)
		{
			const MonitoredEntity& entity = entities[i];

			if (i != 0)
			{
				out << ',';
			}

			out << "{\"id\":";
			String(out, entity.id);
			out << ",\"villageId\":";
			String(out, entity.villageId);
			out << ",\"kind\":";
			String(out, entity.kind);
			out << ",\"behavior\":";
			String(out, entity.behavior);
			out << ",\"state\":";
			String(out, entity.state);
			out << ",\"position\":{\"x\":";
			Number(out, entity.position.x);
			out << ",\"y\":";
			Number(out, entity.position.y);
			out << ",\"z\":";
			Number(out, entity.position.z);
			out << "},\"alive\":" << (entity.alive ? "true" : "false") << ",\"health\":";

			if (entity.hasHealth)
			{
				Number(out, entity.health);
			}
			else
			{
				out << "null";
			}

			out << ",\"maxHealth\":";

			if (entity.hasHealth)
			{
				Number(out, entity.maxHealth);
			}
			else
			{
				out << "null";
			}

			out << ",\"respawnSeconds\":";
			Number(out, entity.respawnSeconds);
			out << '}';
		}

		out << ']';
	}

	std::string Snapshot(const std::string& sourceId, const std::string& instanceId,
		std::uint64_t sequence, const std::vector<MonitoredWorld>& worlds)
	{
		std::ostringstream out;
		out.imbue(std::locale::classic());
		out << std::setprecision(6) << "{\"schemaVersion\":1,\"type\":\"snapshot\",\"sourceId\":";
		String(out, sourceId);
		out << ",\"instanceId\":";
		String(out, instanceId);
		out << ",\"sequence\":" << sequence << ",\"worlds\":[";

		for (size_t i = 0; i < worlds.size(); ++i)
		{
			const MonitoredWorld& world = worlds[i];

			if (i != 0)
			{
				out << ',';
			}

			out << "{\"id\":";
			String(out, world.id);
			out << ",\"name\":";
			String(out, world.name);
			out << ",\"state\":";
			String(out, world.state);
			out << ",\"active\":" << (world.active ? "true" : "false")
				<< ",\"width\":" << world.width << ",\"height\":" << world.height
				<< ",\"kills\":" << world.kills << ",\"villages\":[";

			for (size_t j = 0; j < world.villages.size(); ++j)
			{
				if (j != 0)
				{
					out << ',';
				}

				out << "{\"id\":";
				String(out, world.villages[j].id);
				out << ",\"name\":";
				String(out, world.villages[j].name);
				const MonitoredVillage& village = world.villages[j];

				if (village.hasAtmosphere)
				{
					out << ",\"atmosphere\":{\"mood\":";
					String(out, village.mood);
					out << ",\"fear\":" << village.fear << ",\"trust\":" << village.trust
						<< ",\"prosperity\":" << village.prosperity << ",\"homeNpcCount\":" << village.homeNpcCount
						<< ",\"presentNpcCount\":" << village.presentNpcCount << ",\"travelers\":" << village.travelers
						<< ",\"knownRumors\":" << village.knownRumors << ",\"nearbyMonsters\":" << village.nearbyMonsters << '}';
				}

				out << '}';
			}

			out << "],\"npcs\":";
			Entities(out, world.npcs);
			out << ",\"monsters\":";
			Entities(out, world.monsters);
			out << '}';
		}

		out << "]}";

		return out.str();
	}
}

MonitoringPublisher::~MonitoringPublisher()
{
	Stop();
}

void MonitoringPublisher::Start()
{
	if (m_Enabled || Environment("GAME_MONITOR_ENABLED") == "0")
	{
		return;
	}

	m_SourceId = Environment("GAME_MONITOR_SOURCE");

	if (m_SourceId.empty())
	{
		m_SourceId = "gsw-base-project";
	}

	const std::string portText = Environment("GAME_MONITOR_PORT");
	char* end = NULL;
	const unsigned long port = portText.empty() ? 8765 : strtoul(portText.c_str(), &end, 10);

	if (!IsIdentifier(m_SourceId) || (!portText.empty()
		&& (portText[0] < '0' || portText[0] > '9' || *end != '\0' || port == 0 || port > 65535)))
	{
		AnalysisLog::Get().Event("monitor.config_invalid", "source_or_port;disabled");
		return;
	}

	m_Port = static_cast<unsigned short>(port);
	const auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	m_InstanceId = std::to_string(timestamp) + "-" + std::to_string(Platform::ProcessId());
	m_Stopping = false;
	m_Sequence = 0;
	m_NextPublishMs = 0.0;

	try
	{
		m_Worker = std::thread(&MonitoringPublisher::Run, this);
		m_Enabled = true;
		AnalysisLog::Get().Event("monitor.started", "source=" + m_SourceId + ";instance=" + m_InstanceId
			+ ";port=" + std::to_string(m_Port));
	}
	catch (const std::exception&)
	{
		AnalysisLog::Get().Event("monitor.failed", "worker_creation;disabled");
	}
}

void MonitoringPublisher::Stop()
{
	if (!m_Worker.joinable())
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Stopping = true;
		m_Pending.clear();
	}

	m_Wake.notify_one();
	m_Worker.join();
	m_Enabled = false;
	AnalysisLog::Get().Event("monitor.stopped", "publisher_shutdown");
}

bool MonitoringPublisher::IsDue(double nowMs)
{
	const int connection = m_Connection.load();

	if (connection != m_LastConnection)
	{
		AnalysisLog::Get().Event(connection == 1 ? "monitor.connected" : "monitor.disconnected", "local_receiver");
		m_LastConnection = connection;
	}

	if (!m_Enabled || nowMs < m_NextPublishMs)
	{
		return false;
	}

	m_NextPublishMs = nowMs + PUBLISH_INTERVAL_MS;

	return true;
}

void MonitoringPublisher::Publish(const std::vector<MonitoredWorld>& worlds)
{
	const std::string message = Snapshot(m_SourceId, m_InstanceId, ++m_Sequence, worlds);

	{
		// Only the latest snapshot matters; a stopped receiver cannot grow a queue.
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Pending = message;
	}

	m_Wake.notify_one();
}

void MonitoringPublisher::Run()
{
#ifndef _WIN32
	while (true)
	{
		std::string message;

		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			m_Wake.wait(lock, [this] { return m_Stopping || !m_Pending.empty(); });

			if (m_Stopping)
			{
				break;
			}

			message.swap(m_Pending);
		}

		m_Connection.store(PostSnapshot(m_Port, message) ? 1 : -1);
	}
#else
	HINTERNET session = WinHttpOpen(L"GSW-Monitor/1", WINHTTP_ACCESS_TYPE_NO_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

	if (session == NULL)
	{
		m_Connection.store(-1);
		return;
	}

	WinHttpSetTimeouts(session, HTTP_TIMEOUT_MS, HTTP_TIMEOUT_MS, HTTP_TIMEOUT_MS, HTTP_TIMEOUT_MS);
	HINTERNET connection = WinHttpConnect(session, L"127.0.0.1", m_Port, 0);

	while (true)
	{
		std::string message;

		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			m_Wake.wait(lock, [this] { return m_Stopping || !m_Pending.empty(); });

			if (m_Stopping)
			{
				break;
			}

			message.swap(m_Pending);
		}

		HINTERNET request = connection != NULL ? WinHttpOpenRequest(connection, L"POST", L"/api/snapshot",
			NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0) : NULL;
		bool delivered = false;

		if (request != NULL)
		{
			// Receiver errors and redirects are failures, never alternate destinations.
			DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
			WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));
			DWORD status = 0;
			DWORD statusBytes = sizeof(status);
			const DWORD bytes = static_cast<DWORD>(message.size());
			delivered = WinHttpSendRequest(request, L"Content-Type: application/json; charset=utf-8\r\n", -1L,
				&message[0], bytes, bytes, 0)
				&& WinHttpReceiveResponse(request, NULL)
				&& WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusBytes, WINHTTP_NO_HEADER_INDEX)
				&& status >= 200 && status < 300;
			WinHttpCloseHandle(request);
		}

		m_Connection.store(delivered ? 1 : -1);
	}

	if (connection != NULL)
	{
		WinHttpCloseHandle(connection);
	}

	WinHttpCloseHandle(session);
#endif
}
