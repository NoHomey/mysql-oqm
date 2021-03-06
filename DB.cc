#include <algorithm>
#include <stdexcept>
#include "DB.hh"
#include "toUpper.hh"
#include "Pattern.hh"
#include "Type.hh"

#include <iostream>

DB::DB(const char* Name): name(Name), tables(std::vector<Table*>()), connections(std::vector<Connection>()), inserted(0) {}

std::string DB::merge_name(Table* T1, Table* T2) {
	if(T1->name[0] < T2->name[0]) {
		return T1->name + std::string("_") + T2->name;
	}
	return T2->name + std::string("_") + T1->name;;
}

bool DB::not_in(Table* T) {
	std::vector<Table*>::iterator end = tables.end();
	return std::find(tables.begin(), end, T) == end;
}

void DB::connection(ConnectionType type, Table* T1, Table* T2) {
	if(type == ConnectionType::many_to_many) {
		connections.push_back({.type = type, .from = T1, .to = T2});
	} else {
		connections.insert(connections.begin(), {.type = type, .from = T1, .to = T2});
	}
}

void DB::add_if_missing(Table* T) {
	if(not_in(T)) {
		tables.insert(tables.begin(), T);
	}
}

void DB::many_to_many(Table* T1, Table* T2, Table* T3) {
	std::string name = merge_name(T1, T2);
	T3->name.swap(name);
	T3->key(T2, Type::Key());
	T3->key(T1, Type::Key());
	add_if_missing(T1);
	add_if_missing(T2);
	tables.push_back(T3);
	connection(ConnectionType::many_to_many, T1, T2);
}

void DB::add_tables(Table* T1, Table* T2) {
	if(tables.empty()) {
		tables.push_back(T2);
		tables.push_back(T1);
	} else {
		std::vector<Table*>::iterator end = tables.end();
		std::vector<Table*>::iterator begin = tables.begin();
		std::vector<Table*>::iterator find_T1 = std::find(begin, end, T1);
		std::vector<Table*>::iterator find_T2 = std::find(begin, end, T2);
		if(find_T1 == end) {
			tables.insert(find_T2 + (int)(find_T2 != end), T1);
		} else if(find_T2 == end) {
			tables.insert(find_T1, T2);
		}
	}
}

void DB::many_to_one(Table* T1, Table* T2) {
	T1->key(T2, Type::Key());
	add_tables(T1, T2);
	connection(ConnectionType::many_to_one, T1, T2);
}

void DB::one_to_many(Table* T1, Table* T2) {
	many_to_one(T2, T1);
}

void DB::one_to_one(Table* T1, Table* T2) {
	T1->key(T2, Type::UniqueKey());
	add_tables(T1, T2);
	connection(ConnectionType::one_to_one, T1, T2);
}

std::string DB::create(void) {
  std::string sql_query = toUpper(std::string("drop database if exists "), upper);
  sql_query += name + toUpper(std::string(";\ncreate database "), upper);
  sql_query += name + std::string(";\n") + use();
  for(Table* table: tables) {
	  sql_query += table->create();
  }
  return sql_query;
}

std::string DB::insert(unsigned int count) {
  std::string sql_query("");
  for(Table* table: tables) {
	  sql_query += table->insert(count);
  }
  inserted = count;
  return sql_query;
}

std::string DB::use(void) {
	return toUpper(std::string("use "), upper) + name + std::string(";\n");
}

std::string DB::migrate(Table* T1, std::vector<std::string> fields, Table* T2, std::string name) {
	for(std::string field : fields) {
		std::vector<Field>::iterator it = std::find_if(T2->fields.begin(), T2->fields.end(),
			[field](Field search) -> bool {return search.name == field;});
		T1->fields.push_back(*it);
		T2->fields.erase(it);
	}
	std::string sql_query = T1->create();
	sql_query += toUpper(std::string("\ninsert into "), upper) + T1->name + std::string(" (");
	for(std::string field : fields) {
		sql_query += field + std::string(", ");
	}
	sql_query.pop_back();
	sql_query.pop_back();
	sql_query += toUpper(std::string(") select "), upper);
	for(std::string field : fields) {
		sql_query += field + std::string(", ");
	}
	sql_query.pop_back();
	sql_query.pop_back();
	sql_query += toUpper(std::string(" from "), upper) + T2->name + std::string(";\n");
	for(std::string field : fields) {
		sql_query += toUpper(std::string("alter table "), upper) + T2->name;
		sql_query += toUpper(std::string(" drop column "), upper) + field + std::string(";\n");
	}
	sql_query += toUpper(std::string("alter table "), upper) + T2->name;
	sql_query += toUpper(std::string(" rename "), upper) + name + std::string(";\n");
	T2->name.swap(name);
	return sql_query;
}

std::vector<Connection> DB::get_connections(Table* table) {
	std::vector<Connection> found;
	for(Connection connection: connections) {
		if((connection.from == table) || (connection.to == table)) {
			found.push_back(connection);
		}
	}
	return found;
}

Table* DB::find_connection_table(Connection connection) {
	std::vector<Table*>::iterator it = std::find_if(tables.begin(), tables.end(),
		[this, connection](Table* table) -> bool {
			std::map<Table*, std::string>::iterator end = table->keys.end();
			return ((table->keys.find(connection.from) != end) && (table->keys.find(connection.to) != end));
		}
	);
	return *it;
}

std::string DB::select(Table* wich, Table* given, enum JoinType join_type, unsigned int id) {
	if((id > inserted) && (id > 0)) {
		throw std::invalid_argument("id must be from 1 to number of inserts (including)");
	}
	if((join_type == JoinType::left_excld) || (join_type == JoinType::right_excld)) {
		std::string sql_query = select(wich, given, join_type == JoinType::left_excld ? JoinType::left : JoinType::right);
		sql_query.pop_back();
		sql_query.pop_back();
		sql_query += toUpper(std::string("\nwhere "), upper) + (join_type == JoinType::left_excld ? given->name : wich->name);
		sql_query += std::string(".id ") + toUpper(std::string("is null"), upper);
		return sql_query + std::string(";\n");
	}
	if((join_type == JoinType::outer) || (join_type == JoinType::outer_excld)) {
		std::string sql_query = select(wich, given, JoinType::left);
		sql_query.pop_back();
		sql_query.pop_back();
		sql_query += toUpper(std::string("\nunion all"), upper);
		sql_query += select(wich, given, join_type == JoinType::outer ? JoinType::right : JoinType::right_excld);
		if(join_type == JoinType::outer_excld) {
			sql_query.pop_back();
			sql_query.pop_back();
			sql_query += toUpper(std::string(" or "), upper) + given->name;
			sql_query += std::string(".id ") + toUpper(std::string("is null"), upper);
			return sql_query + std::string(";\n");
		}
		return sql_query;
	}
	std::vector<Table*> search;
	std::vector<Table*> visited;
	std::vector<Table*>::iterator table_end;
	std::vector<Connection> path;
	std::vector<Connection> found;
	std::vector<Connection>::iterator conn_end;
	Table* checked;
	unsigned int size;
	Table* last = wich;
	search.push_back(last);
	bool pushed;
	while(last != given) {
		table_end = visited.end();
		found = get_connections(last);
		if((!found.size()) || (std::find(visited.begin(), table_end, last) != table_end)) {
			search.pop_back();
			if(path.back()[last] == search.back()) {
				search.pop_back();
			}
			last = search.back();
			continue;
		}
		pushed = false;
		visited.push_back(last);
		for(Connection connection: found) {
			conn_end = path.end();
			if(std::find(path.begin(), conn_end, connection) != conn_end) {
				continue;
			}
			checked = connection[last];
			table_end = search.end();
			if(std::find(search.begin(), table_end, checked) != table_end) {
				continue;
			}
			search.push_back(checked);
			path.push_back(connection);
			pushed = true;
			if(checked == given) {
				visited.push_back(checked);
				break;
			}
		}
		if(pushed) {
			last = path.back()[last];
		}
	}
	table_end = visited.end();
	std::vector<Table*>::iterator visited_begin = visited.begin();
	for(Table* table: search) {
		if(std::find(visited_begin, table_end, table) == table_end) {
			search.erase(std::find(search.begin(), search.end(), table));
		}
	}
	found.clear();
	std::vector<ConnectionType> type;
	size = search.size();
	for(unsigned int i = 0; i < size - 1; ++i) {
		path = get_connections(search[i]);
		for(Connection connection: path) {
			conn_end = found.end();
			if((connection[search[i]] == search[i + 1]) && (std::find(found.begin(), conn_end, connection) == conn_end)) {
				found.push_back(connection);
				type.push_back(connection.type);
			}
		}
	}
	for(unsigned int i = 0; i < size - 1; ++i) {
		found[i].type = type[i];
	}
	std::string sql_query = toUpper(std::string("\nselect "), upper) + wich->name + std::string(".id ");
	sql_query += toUpper(std::string("from "), upper) + wich->name + std::string("\n");
	last = wich;
	for(Connection connection: found) {
		std::string join_str;
		switch(join_type) {
			case left:
				join_str = std::string("left");
				break;
			case right:
				join_str = std::string("right");
				break;
			default:
				join_str = std::string("inner");
				break;
		}
		join_str += std::string(" join");
		if(connection.type != ConnectionType::many_to_many) {
			sql_query += toUpper(join_str, upper) + std::string(" ") + connection[last]->name;
			sql_query += toUpper(std::string(" on "), upper) + connection.from->name + std::string(".");
			sql_query += connection.from->keys[connection.to] + std::string(" = ") + connection.to->name + std::string(".id\n");
		} else {
			checked = find_connection_table(connection);
			sql_query += toUpper(join_str, upper) + std::string(" ") + checked->name;
			sql_query += toUpper(std::string(" on "), upper) + checked->name + std::string(".");
			sql_query += checked->keys[last] + std::string(" = ") + last->name + std::string(".id\n");
			sql_query += toUpper(join_str, upper) + std::string(" ") + connection[last]->name;
			sql_query += toUpper(std::string(" on "), upper) + checked->name + std::string(".");
			sql_query += checked->keys[connection[last]] + std::string(" = ") + connection[last]->name + std::string(".id\n");
		}
		last = connection[last];
	}
	if(join_type == JoinType::inner) {
		sql_query += toUpper(std::string("where "), upper) + given->name;
		sql_query += std::string(".id = ") + std::to_string(id);
	} else {
		sql_query.pop_back();
	}
	return sql_query + std::string(";\n");
}
