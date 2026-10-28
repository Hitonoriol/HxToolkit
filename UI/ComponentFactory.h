#pragma once
#include "Enums/ToolType.h"
#include "UI/Component.h"
#include "UI/Tab.h"
#include "HxNxToolkit.h"

#include <QString>
#include <QList>

#include <map>
#include <functional>
#include <optional>

using NamedComponentSupplier = std::function<Component*(HxNxToolkit*, const QString& name)>;
using ComponentSupplier = std::function<Component* (HxNxToolkit*)>;

struct ComponentSupplierEntry
{
	QString CategoryName;
	QString ToolName;
	QString IconPath;
	ComponentSupplier Supplier;
	bool WorkspaceWide;

	ComponentSupplierEntry(const QString& CategoryName, const QString& ToolName, const QString& IconPath,
		NamedComponentSupplier Supplier, bool workspaceWide = false)
		: CategoryName(CategoryName), ToolName(ToolName), IconPath(IconPath), WorkspaceWide(workspaceWide)
	{
		this->Supplier = [ToolName, Supplier](HxNxToolkit* toolkit) {
			return Supplier(toolkit, ToolName);
		};
	}
};

struct ToolInfo
{
	ToolType Type;
	QString CategoryName;
	QString ToolName;
	QString IconPath;
};

class ComponentFactory
{
public:
	static QList<ToolInfo> AvailableTools();
	static std::optional<ToolInfo> GetToolInfo(ToolType toolType);
	static Component* CreateComponent(HxNxToolkit* toolkit, ToolType toolType);

private:
	template<class C>
	static NamedComponentSupplier DefaultSupplier(bool fillContainer = true)
	{
		return [fillContainer](HxNxToolkit* toolkit, const QString& name) {
			auto component = new C;
			auto tab = toolkit->GetCurrentTab();
			if (!tab) {
				tab = toolkit->NewTab();
			}

			tab->AddComponent(component, name, fillContainer);
			return component;
		};
	}

	static std::map<ToolType, ComponentSupplierEntry> componentSuppliers;
};

