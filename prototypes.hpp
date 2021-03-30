#include <string>
#include "csv.hpp"

// user-defined type
struct person
{
	std::string name;
	int age;
};

// prototype that define how to serialize and deserialize a user-defined type
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
