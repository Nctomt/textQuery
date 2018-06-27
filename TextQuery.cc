 ///
 /// @file    TextQuery.cc
 /// @author  Nctomt(62526344@qq.com)
 /// @date    2018-06-27 17:38:11
 ///
 
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <set>
#include <map>
#include <iostream>
#include <algorithm>

using std::cout;
using std::endl;
using std::vector;
using std::string;
using std::ifstream;
using std::shared_ptr;
using std::map;
using std::set;
using std::istringstream;
using std::cin;
using std::make_shared;

class QueryResult;
class TextQuery
{
public:
	using line_no = vector<string>::size_type;
	TextQuery(ifstream&);
	QueryResult query(const string&) const;

private:
	shared_ptr<vector<string>> file;
	map<string, shared_ptr<set<line_no>>> wm;
};

TextQuery::TextQuery(ifstream & is)
: file(new vector<string>)
{	
	string text;
	while(getline(is, text))
	{
		file->push_back(text);
		int n = file->size() - 1;
		istringstream line(text);
		string word;
		while(line >> word)
		{
			auto & lines = wm[word];
			if(!lines)
				lines.reset(new set<line_no>);
			lines->insert(n);
		}
	}
}

class QueryResult
{
	friend std::ostream & print(std::ostream &, const QueryResult &);
public:
	using line_no = vector<string>::size_type;
	QueryResult(string s, shared_ptr<set<line_no>> p, 
			shared_ptr<vector<string>> f)
	: sought(s)
	, lines(p)
	, file(f)
	{}
	
	set<line_no>::const_iterator begin()
	{	return lines->begin();	}

	set<line_no>::const_iterator end()
	{	return lines->end();		}

	shared_ptr<vector<string>> get_file()
	{	return file;			}



private:
	string sought;
	shared_ptr<set<line_no>> lines;
	shared_ptr<vector<string>> file;
};

QueryResult TextQuery::query(const string & sought) const
{
	static shared_ptr<set<line_no>> nodata(new set<line_no>);
	auto loc = wm.find(sought);
	if(loc == wm.end())
		return QueryResult(sought, nodata, file);
	else
		return QueryResult(sought, loc->second, file);
}

std::ostream & print(std::ostream & os, const QueryResult & qr)
{
	os << qr.sought << "occur" << qr.lines->size()
	   << " " << (qr.lines->size() > 1 ? "times" : "time" ) << endl;
	for(auto num : * qr.lines)
	{
		os << "\t(line " << num + 1 << ")" 
		   << *(qr.file->begin() + num) << endl;
	}

	return os;

}

class Query_base
{//查询基类  具体的查询类型从中派生。所有的成员都是private的
	friend class Query;
	
protected:
	using line_no = TextQuery::line_no;
	virtual ~Query_base() = default;

private:
	//eval返回当前与之匹配的QueryResult
	virtual QueryResult eval(const TextQuery &) const = 0;
	//rep表示查询的一个string
	virtual string rep() const = 0;
};

class Query
{	//管理Query继承体系的类		
	friend Query operator~(const Query &);
	friend Query operator|(const Query &, const Query &);
	friend Query operator&(const Query &, const Query &);

public:
	Query(const string &);

	//接口对应Query_base 操作
	QueryResult eval(const TextQuery &t) const
	{	 return q->eval(t);		}
	
	string rep() const
	{	 return q->rep();		}

private:
	Query(shared_ptr<Query_base> query)
	: q(query)
	{}
	shared_ptr<Query_base> q;
};

//Query 的输出运算符
std::ostream & operator<< (std::ostream & os, const Query & query)
{	return os << query.rep();	}

class WordQuery
: public Query_base
{
	friend class Query;			//Query 使用 WordQuery构造函数

	WordQuery(const string & s) //wordQuery将定义所以继承而来的虚函数
	:query_word(s)
	{}

	QueryResult eval(const TextQuery & t) const
	{	return t.query(query_word);		}

	string rep() const
	{	return query_word;		}

	string query_word;
};

inline
Query::Query(const string & s)
: q(new WordQuery(s)) {}



class NotQuery
: public Query_base
{
	friend Query operator~(const Query &);

	NotQuery(const Query & q)
	: query(q)
	{}

	string rep() const
	{	return "~(" + query.rep() + ")";	}

	QueryResult eval(const TextQuery &) const;
	Query query;
};

inline
Query operator~(const  Query & operand)
{	return shared_ptr<Query_base>(new NotQuery(operand));	}

class BinaryQuery
: public Query_base
{
protected:
	BinaryQuery(const Query & l, const Query & r, string s)
	: lhs(l)
	, rhs(r)
	, opSym(s)
	{}

	string rep() const 
	{	return "(" + lhs.rep() + " " + opSym + " " + rhs.rep() + ")";	}

	Query lhs, rhs;
	string opSym;
};



class AndQuery
: public BinaryQuery
{
	friend Query operator&(const Query &, const Query &);

	AndQuery(const Query & left, const Query & right)
	: BinaryQuery(left, right, "&")
	{}

	QueryResult eval(const TextQuery &) const;
};

inline
Query operator &(const Query & lhs, const Query & rhs)
{	return shared_ptr<Query_base>(new AndQuery(lhs, rhs));	}


class OrQuery
: public BinaryQuery
{
	friend Query operator|(const Query &, const Query &);

	OrQuery(const Query & left, const Query & right)
	:	BinaryQuery(left, right, "|")
	{}

	QueryResult eval(const TextQuery &) const;
};

inline Query operator|(const Query & lhs, const Query & rhs)
{	return shared_ptr<Query_base>(new OrQuery(lhs, rhs));	}

QueryResult OrQuery::eval(const TextQuery & text) const
{
	auto right = rhs.eval(text), left = lhs.eval(text);
	auto ret_lines = make_shared<set<line_no>>(left.begin(), left.end());
	ret_lines->insert(right.begin(), right.end());
	return QueryResult(rep(), ret_lines, left.get_file());
}

QueryResult AndQuery::eval(const TextQuery & text) const
{
	auto left = lhs.eval(text), right = rhs.eval(text);
	auto ret_lines = make_shared<set<line_no>>();
	set_intersection(left.begin(), left.end(), right.begin(),right.end(), inserter(*ret_lines, ret_lines->begin()));
	return QueryResult(rep(), ret_lines, left.get_file());
}

QueryResult NotQuery::eval(const TextQuery & text) const
{
	auto result = query.eval(text);
	auto ret_lines = make_shared<set<line_no>>();
	auto beg = result.begin(), end = result.end();
	auto sz = result.get_file()->size();
	for(size_t n = 0; n != sz; ++n)
	{
		if(beg == end || *beg != n)
			ret_lines->insert(n);
		else if(beg != end)
			++beg;
	}
	return QueryResult(rep(), ret_lines, result.get_file());
}



int main(int argc, char *argv[])
{
	if(argc != 2)
	{ 
		cout << "error args!" << endl;
		return 0;
	}

	ifstream ifs(argv[1]);
	TextQuery tq(ifs);
	Query q = Query("fiery") & Query("bird") | Query("wind");
	print(cout, q.eval(tq));
	
	return 0;
}



