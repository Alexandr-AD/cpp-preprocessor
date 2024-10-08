#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char *data, std::size_t sz)
{
    return path(data, data + sz);
}

const regex INC_DIR_REG(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
const regex INC_CUR_DIR_REG(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");

bool Preprocess(ifstream &in_file, ofstream &out_file, const path &in_file_name, const vector<path> &include_directories)
{
    smatch match;
    string read_str;
    size_t num_str = 0;
    bool flag = false;
    if (!in_file.is_open())
    {
        return false;
    }
    while (getline(in_file, read_str))
    {
        ++num_str;
        if (regex_match(read_str, match, INC_DIR_REG))
        {
            path local_path = in_file_name.parent_path() / string(match[1]);
            ifstream in_tmp1(local_path);
            if (!Preprocess(in_tmp1, out_file, local_path, include_directories))
            {
                local_path = string(match[1]);
                for (const auto &dir : include_directories)
                {
                    path d = dir / local_path;
                    ifstream subdir(d);
                    if (subdir.is_open())
                    {
                        flag = true;
                        Preprocess(subdir, out_file, d, include_directories);
                        break;
                    }
                }
                if (!flag)
                {
                    cout << "unknown include file "s << string(match[1]) << " at file "s << in_file_name.string() << " at line "s << num_str << endl;
                    return false;
                }
            }
        }
        else if (regex_match(read_str, match, INC_CUR_DIR_REG))
        {
            path local_path = string(match[1]);
            for (const auto &dir : include_directories)
            {
                path d = dir / local_path;
                ifstream subdir(d);
                if (subdir.is_open())
                {
                    flag = true;
                    Preprocess(subdir, out_file, d, include_directories);
                    break;
                }
            }
            if (!flag)
            {
                cout << "unknown include file "s << string(match[1]) << " at file "s << in_file_name.string() << " at line "s << num_str << endl;
                return false;
            }
        }
        if (!(regex_match(read_str, INC_DIR_REG) || regex_match(read_str, INC_CUR_DIR_REG)))
        {
            out_file << read_str << endl;
        }
    }
    return true;
}

bool Preprocess(const path &in_file, const path &out_file, const vector<path> &include_directories)
{
    ifstream in(in_file);
    if (!in.is_open())
    {
        return false;
    }
    ofstream out(out_file);
    return Preprocess(in, out, in_file, include_directories);

    return true;
}

string GetFileContents(string file)
{
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test()
{
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                        {"sources"_p / "include1"_p, "sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main()
{
    Test();
}