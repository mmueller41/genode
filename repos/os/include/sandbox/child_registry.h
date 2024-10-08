/*
 * \brief  Child registry
 * \author Norman Feske
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__SANDBOX__CHILD_REGISTRY_H_
#define _LIB__SANDBOX__CHILD_REGISTRY_H_

/* local includes */
#include <sandbox/child.h>
#include <sandbox/name_registry.h>
#include <sandbox/alias.h>
#include <sandbox/report.h>

namespace Sandbox { struct Child_registry; }


class Sandbox::Child_registry : public Name_registry, Child_list
{
	private:

		List<Alias> _aliases { };

	public:

		/**
		 * Register child
		 */
		void insert(Child *child)
		{
			Child_list::insert(&child->_list_element);
		}

		/**
		 * Unregister child
		 */
		void remove(Child *child)
		{
			Child_list::remove(&child->_list_element);
		}

		/**
		 * Register alias
		 */
		void insert_alias(Alias *alias)
		{
			_aliases.insert(alias);
		}

		/**
		 * Unregister alias
		 */
		void remove_alias(Alias *alias)
		{
			_aliases.remove(alias);
		}

		template <typename FN>
		void for_each_child(FN const &fn) const
		{
			Genode::List_element<Child> const *curr = first();
			for (; curr; curr = curr->next())
				fn(*curr->object());
		}

		template <typename FN>
		void for_each_child(FN const &fn)
		{
			Genode::List_element<Child> *curr = first(), *next = nullptr;
			for (; curr; curr = next) {
				next = curr->next();
				fn(*curr->object());
			}
		}

		void report_state(Xml_generator &xml, Report_detail const &detail) const
		{
			for_each_child([&] (Child &child) { child.report_state(xml, detail); });

			for (Alias const *a = _aliases.first(); a; a = a->next()) {
				xml.node("alias", [&] () {
					xml.attribute("name", a->name);
					xml.attribute("child", a->child);
				});
			}
		}

		Child::Sample_state_result sample_state()
		{
			auto result = Child::Sample_state_result::UNCHANGED;

			for_each_child([&] (Child &child) {
				if (result == Child::Sample_state_result::UNCHANGED)
					result = child.sample_state(); });

			return result;
		}

		Child::Name deref_alias(Child::Name const &name) override
		{
			for (Alias const *a = _aliases.first(); a; a = a->next())
				if (name == a->name)
					return a->child;

			return name;
		}
};

#endif /* _LIB__SANDBOX__CHILD_REGISTRY_H_ */
