#pragma once
#include "Enums/ToolType.h"
#include "UI/Component.h"
#include "UI/Tab.h"
#include "HxNxToolkit.h"

#include <QString>

#include <map>
#include <functional>

using NamedComponentSupplier = std::function<Component*(HxNxToolkit*, const QString& name)>;
using ComponentSupplier = std::function<Component* (HxNxToolkit*)>;

struct ComponentSupplierEntry
{
	QString CategoryName;
	QString ToolName;
	ComponentSupplier Supplier;

	ComponentSupplierEntry(const QString& CategoryName, const QString& ToolName, NamedComponentSupplier Supplier)
		: CategoryName(CategoryName), ToolName(ToolName)
	{
		this->Supplier = [ToolName, Supplier](HxNxToolkit* toolkit) {
			return Supplier(toolkit, ToolName);
		};
	}
};

class ComponentFactory
{
public:
	static void Register(HxNxToolkit* toolkit);
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

