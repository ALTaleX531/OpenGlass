#pragma once

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/statline.h>
#include <wx/valnum.h>
#include <wx/clrpicker.h>
#include <wx/filepicker.h>
#include <wx/spinctrl.h>
#include <wx/slider.h>
#include <wx/gauge.h>
#include <wx/commandlinkbutton.h>
#include <wx/collpane.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>
#include <wx/artprov.h>
#include <wx/simplebook.h>
#include <wx/snglinst.h>
#include <wx/tglbtn.h>
#include <wx/wrapsizer.h>
#include <wil/resource.h>
#include <wil/registry.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <shlobj.h>

#include <string>
#include <memory>
#include <functional>
#include <format>
#include <map>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#include <algorithm>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shell32.lib")
