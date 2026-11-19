// Driver for upstream's plugins/transformer/tests/ConfigTest.cpp, which is compiled with
// -Dmain=transformer_config_test_main. Its checks are bare assert()s (abort on failure), so
// it reports only through its exit status; this prints a positive line once every assert has
// passed, which mayhem/test.sh requires (an exit(0)-neutered binary never prints it).
#include <cstdio>

int transformer_config_test_main(int argc, char **argv);

int main(int argc, char **argv)
{
	const int rc = transformer_config_test_main(argc, argv);
	if (rc == 0)
		std::printf("transformer_config_test: all assertions passed\n");
	return rc;
}
