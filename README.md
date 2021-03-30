# csv-cpp

Header only csv-parser library that layout user-defined types using prototype classes.

## Usage

Writing data using a custom prototype to serialize a user-defined type.
 
```cpp
csv::write<person, person_prototype>(
	"persons.csv",
	{
		{"Bin", 3},
		{"Ben", 5},
	},
	{ "Names", "Age" }
);
```

Reading data using a custom prototype to deserialize a user-defined type. Default reading method is asynchronous.

```cpp
auto document_custom = csv::read_from_file<person, person_prototype>("persons.csv");
```

## Prototypes

A prototype is a mean to tell the library how to serialize and deserialize user-defined types like below:

```cpp
struct person
{
	std::string name;
	int age;
};
```

The prototype for a user-defined type T shoud derive from csv::row_base<T> and implement the serialize AND/OR deserialize methods.

```cpp
class person_prototype : public csv::prototype<person>
{
public:
	// to implement for saving data into a csv file
	virtual void serialize(std::stringstream& buffer, const person& data) const override
	{
		buffer << data.name << prototype::get_delimiter();
		buffer << data.age << std::endl;
	}

	// to implement for loading data from a csv file
	virtual person deserialize(std::stringstream& buffer) const override
	{
		person p;
		std::getline(buffer, p.name, prototype::get_delimiter());
		buffer >> p.age;
		return p;
	}
};
```

## Experimental

Writing single type data into a csv file 

```cpp
csv::write
	<
	std::vector<float>,
	csv::experimental::single_type_prototype<float>
	>
	("single_type.csv",
	{
		{1, 1, 1},
		{2, 2, 2},
	},
	{ "A", "B", "C" }
);
```

Reading single type data from a csv file. Currently only supporting [int, float, strings]

```cpp
auto document = csv::read_from_file
	<
	std::vector<float>,
	csv::experimental::single_type_prototype<float>
	>
	("single_type.csv");

return 0;
```
