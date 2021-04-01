#include <vector>
#include "prototypes.hpp"

int main()
{
	// writing data using a custom prototype to serialize a user-defined type
	try {
		csv::write<person, person_prototype>(
			"persons.csv",
			{
				{"Bin", 3},
				{"Ben", 5},
			},
			{ "Names", "Age" }
		);
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
	}


	// reading data using a custom prototype to deserialize a user-defined type
	try {
		auto document = csv::read_from_file<person, person_prototype>("persons.csv");
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
	}


	/* experimental */

	// writing single type data into a csv file 
	try {
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
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
	}

	// reading single type data from a csv file, currently only supporting [int, float, strings]
	try {
		auto document = csv::read_from_file
			<
			std::vector<float>,
			csv::experimental::single_type_prototype<float>
			>
			("single_type.csv");
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
	}

	return 0;
}