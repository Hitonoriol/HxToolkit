#include "WebView2Widget.h"

#ifdef _WIN32

#include <QLabel>
#include <QPointer>
#include <QResizeEvent>
#include <QUrl>
#include <QVBoxLayout>

#include <WebView2.h>
#include <windows.h>

#include <atomic>
#include <functional>
#include <utility>

namespace
{
	template<typename T>
	class ComPtr
	{
	public:
		ComPtr() = default;
		explicit ComPtr(T* value) : value(value) {}
		~ComPtr() { Reset(); }

		ComPtr(const ComPtr&) = delete;
		ComPtr& operator=(const ComPtr&) = delete;

		T* Get() const { return value; }
		T* operator->() const { return value; }
		explicit operator bool() const { return value != nullptr; }

		T** Put()
		{
			Reset();
			return &value;
		}

		void Reset(T* newValue = nullptr)
		{
			if (value) {
				value->Release();
			}
			value = newValue;
		}

	private:
		T* value = nullptr;
	};

	template<typename Interface, const IID* InterfaceId, typename Result>
	class CompletionHandler final : public Interface
	{
	public:
		using Callback = std::function<HRESULT(HRESULT, Result*)>;

		explicit CompletionHandler(Callback callback) : callback(std::move(callback)) {}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override
		{
			if (!object) return E_POINTER;
			if (IsEqualIID(id, IID_IUnknown) || IsEqualIID(id, *InterfaceId)) {
				*object = static_cast<Interface*>(this);
				AddRef();
				return S_OK;
			}

			*object = nullptr;
			return E_NOINTERFACE;
		}

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return ++references;
		}

		ULONG STDMETHODCALLTYPE Release() override
		{
			const auto remaining = --references;
			if (!remaining) delete this;
			return remaining;
		}

		HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, Result* value) override
		{
			return callback(result, value);
		}

	private:
		std::atomic<ULONG> references{1};
		Callback callback;
	};

	using EnvironmentHandler = CompletionHandler<
		ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
		&IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
		ICoreWebView2Environment>;
	using ControllerHandler = CompletionHandler<
		ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
		&IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
		ICoreWebView2Controller>;

	QString HResultMessage(HRESULT result)
	{
		wchar_t* message = nullptr;
		const auto size = FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			result,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<wchar_t*>(&message),
			0,
			nullptr);

		const auto detail = size ? QString::fromWCharArray(message, static_cast<int>(size)).trimmed() : QString{};
		if (message) LocalFree(message);
		return detail.isEmpty()
			? QString("HRESULT 0x%1").arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'))
			: detail;
	}
}

class WebView2Widget::Implementation
{
public:
	explicit Implementation(WebView2Widget* owner)
		: owner(owner)
		, status(new QLabel("Initializing WebView2...", owner))
	{
		status->setAlignment(Qt::AlignCenter);
		auto* layout = new QVBoxLayout(owner);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(status);

		owner->setAttribute(Qt::WA_NativeWindow);
		const auto oleResult = OleInitialize(nullptr);
		if (SUCCEEDED(oleResult)) {
			oleInitialized = true;
		} else if (oleResult == RPC_E_CHANGED_MODE) {
			Fail("WebView2 requires the UI thread to use a single-threaded COM apartment.");
			return;
		} else {
			Fail(QString("Unable to initialize COM for WebView2: %1").arg(HResultMessage(oleResult)));
			return;
		}

		const QPointer<WebView2Widget> guard(owner);
		ComPtr<EnvironmentHandler> handler(new EnvironmentHandler(
			[this, guard](HRESULT result, ICoreWebView2Environment* value) {
				if (!guard) return S_OK;
				return EnvironmentCreated(result, value);
			}));

		const auto result = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, handler.Get());
		if (FAILED(result)) Fail(QString("Unable to start WebView2: %1").arg(HResultMessage(result)));
	}

	~Implementation()
	{
		if (controller) controller->Close();
		webView.Reset();
		controller.Reset();
		environment.Reset();
		if (oleInitialized) OleUninitialize();
	}

	void SetUrl(const QUrl& value)
	{
		url = value;
		if (!webView || !url.isValid()) return;

		const auto encoded = url.toString(QUrl::FullyEncoded).toStdWString();
		webView->Navigate(encoded.c_str());
	}

	void SetUserAgent(const QString& value)
	{
		userAgent = value;
		ApplyUserAgent();
	}

	void Reload()
	{
		if (webView) webView->Reload();
	}

	void Resize()
	{
		if (!controller) return;
		RECT bounds{};
		GetClientRect(reinterpret_cast<HWND>(owner->winId()), &bounds);
		controller->put_Bounds(bounds);
	}

private:
	HRESULT EnvironmentCreated(HRESULT result, ICoreWebView2Environment* value)
	{
		if (FAILED(result) || !value) {
			Fail(QString("Unable to initialize WebView2: %1").arg(HResultMessage(result)));
			return S_OK;
		}

		value->AddRef();
		environment.Reset(value);

		const QPointer<WebView2Widget> guard(owner);
		ComPtr<ControllerHandler> handler(new ControllerHandler(
			[this, guard](HRESULT controllerResult, ICoreWebView2Controller* controllerValue) {
				if (!guard) return S_OK;
				return ControllerCreated(controllerResult, controllerValue);
			}));

		const auto createResult = environment->CreateCoreWebView2Controller(
			reinterpret_cast<HWND>(owner->winId()), handler.Get());
		if (FAILED(createResult)) {
			Fail(QString("Unable to create the WebView2 control: %1").arg(HResultMessage(createResult)));
		}
		return S_OK;
	}

	HRESULT ControllerCreated(HRESULT result, ICoreWebView2Controller* value)
	{
		if (FAILED(result) || !value) {
			Fail(QString("Unable to create the WebView2 control: %1").arg(HResultMessage(result)));
			return S_OK;
		}

		value->AddRef();
		controller.Reset(value);
		const auto webViewResult = controller->get_CoreWebView2(webView.Put());
		if (FAILED(webViewResult) || !webView) {
			Fail(QString("Unable to access the WebView2 control: %1").arg(HResultMessage(webViewResult)));
			return S_OK;
		}

		status->hide();
		controller->put_IsVisible(TRUE);
		Resize();
		ApplyUserAgent();
		SetUrl(url);
		return S_OK;
	}

	void ApplyUserAgent()
	{
		if (!webView) return;

		ComPtr<ICoreWebView2Settings> settings;
		if (FAILED(webView->get_Settings(settings.Put()))) return;

		ComPtr<ICoreWebView2Settings2> settings2;
		if (FAILED(settings->QueryInterface(
			IID_ICoreWebView2Settings2,
			reinterpret_cast<void**>(settings2.Put())))) return;

		const auto value = userAgent.toStdWString();
		settings2->put_UserAgent(value.c_str());
	}

	void Fail(const QString& message)
	{
		status->setText(message + "\n\nInstall or repair the Microsoft Edge WebView2 Runtime and try again.");
		status->show();
	}

	WebView2Widget* owner;
	QLabel* status;
	bool oleInitialized = false;
	QUrl url;
	QString userAgent;
	ComPtr<ICoreWebView2Environment> environment;
	ComPtr<ICoreWebView2Controller> controller;
	ComPtr<ICoreWebView2> webView;
};

WebView2Widget::WebView2Widget(QWidget* parent)
	: QWidget(parent)
	, implementation(std::make_unique<Implementation>(this))
{
}

WebView2Widget::~WebView2Widget() = default;

void WebView2Widget::SetUrl(const QUrl& url)
{
	implementation->SetUrl(url);
}

void WebView2Widget::SetUserAgent(const QString& userAgent)
{
	implementation->SetUserAgent(userAgent);
}

void WebView2Widget::Reload()
{
	implementation->Reload();
}

void WebView2Widget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	implementation->Resize();
}

#endif
