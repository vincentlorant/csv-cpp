#include <vector>
#include "prototypes.hpp"

int main()
{
	// writing data using a custom prototype to serialize a user-defined type
	csv::write<person, person_prototype>(
		"persons.csv",
		{
			{"Bin", 3},
			{"Ben", 5},
		},
		{ "Names", "Age" }
	);

	// reading data using a custom prototype to deserialize a user-defined type
	// default reading method is asynchronous
	auto document_custom = csv::read_from_file<person, person_prototype>("persons.csv");


	/* experimental */

	// writing single type data into a csv file 
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

	// reading single type data from a csv file
	// currently only supporting [int, float, strings]
	auto document = csv::read_from_file
		<
		std::vector<float>,
		csv::experimental::single_type_prototype<float>
		>
		("single_type.csv");

	return 0;
}