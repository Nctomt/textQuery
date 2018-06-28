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
	shared_ptr<vector<string>> file;			//输入文件
	map<string, shared_ptr<set<line_no>>> wm;	//每个单词到所在行号集合的映射
};

//TextQuery构造函数：接收一个ifstream 逐行读取文件
TextQuery::TextQuery(ifstream & is)			
: file(new vector<string>)
{	
	string text;
	while(getline(is, text))//循环对文件的每一行进行操作
	{
		file->push_back(text);		//保存文本
		int n = file->size() - 1;	//行号
		istringstream line(text);	//文本分解为单词
		string word;				
		while(line >> word)//循环对此行中每一个单词操作
		{
			//如果单词不再wm中  把它作为下标 添加到wm中 
			auto & lines = wm[word];//lines是一个shared_ptr
			if(!lines)				//第一次出现某个单词 指针为空
				lines.reset(new set<line_no>);//分配新set
			lines->insert(n);		//将行号插入到set中
		}
	}
}

class QueryResult
{
	friend std::ostream & print(std::ostream &, const QueryResult &);
public:
	using line_no = vector<string>::size_type;
	QueryResult(string s,
				shared_ptr<set<line_no>> p,
				shared_ptr<vector<string>> f)
	//构造函数：string保存查询单词 -- s   
	//shared_ptr保存单词出现行号的set -- p   
	//shared_ptr指向保存输入文件的vector -- f
	: sought(s)
	, lines(p)
	, file(f)
	{}
	
	set<line_no>::const_iterator begin()
	{	return lines->begin();		}

	set<line_no>::const_iterator end()
	{	return lines->end();		}

	shared_ptr<vector<string>> get_file()
	{	return file;				}

private:
	string sought;
	shared_ptr<set<line_no>> lines;
	shared_ptr<vector<string>> file;
};

//接受一个string  查询单词
QueryResult TextQuery::query(const string & sought) const
{
	//未找到string（查询单词） 返回一个指向此的set指针
	static shared_ptr<set<line_no>> nodata(new set<line_no>);
	//使用find而不是下标 ：避免将单词添加到wm中
	auto loc = wm.find(sought);
	if(loc == wm.end())//未找到
		return QueryResult(sought, nodata, file);
	else
		return QueryResult(sought, loc->second, file);
}
//打印结果:在给定的流上打印出给定的QueryResult对象
std::ostream & print(std::ostream & os, const QueryResult & qr)
{
	//打印所以出现次数 以及行号
	os << qr.sought << " occurs " << qr.lines->size()
	   << " " << (qr.lines->size() > 1 ? "times:" : "time:" ) << endl;
	for(auto num : * qr.lines)	//对set中的每一个单词
	{
		os << " (line " << num + 1 << ")" 
		   << *(qr.file->begin() + num) << endl;
	}
	return os;
}


//功能2.0 ：在查询单词的基础上，增加了与或非查询功能
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
{	//管理Query继承体系的类   是Query_base的唯一接口		
	friend Query operator~(const Query &);
	friend Query operator|(const Query &, const Query &);
	friend Query operator&(const Query &, const Query &);

public:
	Query(const string &);			//构建一个新的wordQuery

	//接口对应Query_base 操作
	QueryResult eval(const TextQuery &t) const
	{	 return q->eval(t);		}
	
	string rep() const
	{	 return q->rep();		}

private:
	//不希望一般用户能随便定义Query_base对象
	//所以讲三个运算符声明为友元
	Query(shared_ptr<Query_base> query)
	: q(query)
	{}
	shared_ptr<Query_base> q;
};

//Query 的输出运算符
std::ostream & operator<< (std::ostream & os, const Query & query)
{	return os << query.rep();	}


//保存要查找的单词
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

	string query_word;			//要查找的单词
};
//定义了WordQuery类 就能定义接收string的Query构造函数了
//构造函数分配一个WordQuery 然后领指针成员指向新分配的对象
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
//将得到的NotQuery指针绑定到一个shared_ptr<Query_base>
inline
Query operator~(const  Query & operand)
{	return shared_ptr<Query_base>(new NotQuery(operand));	}



//抽象基类 保存操作两个运算对象的查询类型所需数据
//用于| 和 &
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

	Query lhs, rhs;		//左右测运算对象
	string opSym;		//运算符名字
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
	//通过lhs  rhs 进行虚调用
	auto right = rhs.eval(text), left = lhs.eval(text);
	//将左侧运算对象的结果  拷贝到结果set中
	auto ret_lines = make_shared<set<line_no>>(left.begin(), left.end());
	//插入右侧运算对象所得行号
	ret_lines->insert(right.begin(), right.end());
	//返回rhs 和 lhs 并集
	return QueryResult(rep(), ret_lines, left.get_file());
	//返回值：表示查询的string   
	//		  匹配行号set的shared_ptr
	//		  指向输入文件的vector的shared_ptr
}

QueryResult AndQuery::eval(const TextQuery & text) const
{	//和Or的差别是  调用了标准库算法来查询结果的交集（行）
	auto left = lhs.eval(text), right = rhs.eval(text);
	//标准库算法 取交集
	auto ret_lines = make_shared<set<line_no>>();
	//将两个范围的交集写入一个目标迭代器中
	//标准库算法 set_intersection合并两个set
	set_intersection(left.begin(), left.end(),
					 right.begin(),right.end(), 
					 inserter(*ret_lines, ret_lines->begin()));
	return QueryResult(rep(), ret_lines, left.get_file());
}

QueryResult NotQuery::eval(const TextQuery & text) const
{
	//通过Query运算对象对eval进行虚调用
	auto result = query.eval(text);
	auto ret_lines = make_shared<set<line_no>>();
	auto beg = result.begin(), end = result.end();//必须对所有行进行迭代
	//对输入文件每一行， 如果不在result中  添加到ret_lines中
	auto sz = result.get_file()->size();
	for(size_t n = 0; n != sz; ++n)
	{
		if(beg == end || *beg != n)
			ret_lines->insert(n);				 //如果不存在 添加到此
		else if(beg != end)
			++beg;								 //存在 获取下一行
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

	Query q1 = Query("Daddy");
	print(cout, q1.eval(tq));

	cout << endl;
	Query q2 = ~Query("Alice");
	print(cout, q2.eval(tq));

	cout << endl;
	Query q3 = Query("hair") | Query("Alice");
	print(cout, q3.eval(tq));

	cout << endl;
	Query q4 = Query("hair") & Query("Alice");
	print(cout, q4.eval(tq));

	cout << endl;
	Query q = Query("fiery") & Query("bird") | Query("wind");
	print(cout, q.eval(tq));
	
	return 0;
}



