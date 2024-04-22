#include "Liquid.hpp"

int main(int argc, char *argv[])
{
  std::unique_ptr<Liquid> liquid(new Liquid(argc, argv));
  liquid->run();
}
