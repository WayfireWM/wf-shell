#include "category.hpp"

WfMenuCategory::WfMenuCategory(std::string _name, std::string _icon_name) :
    name(_name), icon_name(_icon_name)
{}

std::string WfMenuCategory::get_name()
{
    return name;
}

std::string WfMenuCategory::get_icon_name()
{
    return icon_name;
}
