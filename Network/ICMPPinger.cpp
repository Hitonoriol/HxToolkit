#include "ICMPPinger.h"

#include <QHostInfo>
#include <QAbstractSocket>
#include <QtConcurrentRun>

#include <array>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <iphlpapi.h> // IPAddr and ICMP_ECHO_REPLY
#include <icmpapi.h>  // IcmpCreateFile, IcmpSendEcho, IcmpCloseHandle
#endif

ICMPPinger::ICMPPinger(QObject* parent)
	: QObject(parent)
{
	connect(&watcher, &QFutureWatcher<ICMPPingResult>::finished, this, [this] {
		emit PingFinished(watcher.result());
	});
}

void ICMPPinger::Ping(const QString& host, int timeoutMs)
{
	if (watcher.isRunning() || host.isEmpty()) {
		return;
	}

	watcher.setFuture(QtConcurrent::run([host, timeoutMs] {
		ICMPPingResult result;
		auto hostInfo = QHostInfo::fromName(host);
		for (const auto& address : hostInfo.addresses()) {
			if (address.protocol() != QAbstractSocket::IPv4Protocol) {
				continue;
			}

#ifdef Q_OS_WIN
			auto handle = IcmpCreateFile();
			if (handle == INVALID_HANDLE_VALUE) {
				return result;
			}

			std::array<std::byte, sizeof(ICMP_ECHO_REPLY) + 32> replyBuffer{};
			const auto replies = IcmpSendEcho(
				handle,
				htonl(address.toIPv4Address()),
				nullptr,
				0,
				nullptr,
				replyBuffer.data(),
				static_cast<DWORD>(replyBuffer.size()),
				static_cast<DWORD>(timeoutMs));
			IcmpCloseHandle(handle);

			if (replies > 0) {
				auto* reply = reinterpret_cast<const ICMP_ECHO_REPLY*>(replyBuffer.data());
				result.online = reply->Status == IP_SUCCESS;
				result.roundTripTimeMs = static_cast<int>(reply->RoundTripTime);
			}
#endif
			return result;
		}
		return result;
	}));
}

bool ICMPPinger::IsRunning() const
{
	return watcher.isRunning();
}
