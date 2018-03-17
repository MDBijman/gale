#pragma once
#include "fe/data/runtime_environment.h"
#include "fe/data/type_environment.h"
#include "fe/modes/project.h"
#include "fe/data/values.h"
#include "fe/data/types.h"
#include <Windows.h>

namespace fe
{
	namespace stdlib
	{
		namespace ui
		{
			const auto WndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
			{
				switch (msg)
				{
				case WM_CLOSE:
					DestroyWindow(hwnd);
					break;
				case WM_DESTROY:
					PostQuitMessage(0);
					break;
				default:
					return DefWindowProc(hwnd, msg, wParam, lParam);
				}
				return 0;
			};

			static std::tuple<type_environment, runtime_environment, scope_environment> load()
			{
				scope_environment se{};
				type_environment te{};
				runtime_environment re{};

				using namespace fe::types;
				using namespace fe::values;

				// Create Window
				{
					function_type create_window_type{
						atom_type{"std.str"},
						atom_type{"HWindow"}
					};

					se.declare(extended_ast::identifier({ "create_window" }), extended_ast::identifier({ "_function" }));
					se.define(extended_ast::identifier({ "create_window" }));
					te.set_type(extended_ast::identifier({ "create_window" }),
						fe::types::make_unique(create_window_type));
					re.set_value("create_window", native_function([](unique_value t) -> unique_value {
						auto name = dynamic_cast<string*>(t.get())->val;

						WNDCLASSEX wc;
						HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);

						//Step 1: Registering the Window Class
						wc.cbSize = sizeof(WNDCLASSEX);
						wc.style = 0;
						wc.lpfnWndProc = WndProc;
						wc.cbClsExtra = 0;
						wc.cbWndExtra = 0;
						wc.hInstance = hInstance;
						wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
						wc.hCursor = LoadCursor(NULL, IDC_CROSS);
						wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
						wc.lpszMenuName = NULL;
						wc.lpszClassName = "MyWC";
						wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

						if (!RegisterClassEx(&wc))
						{
							MessageBox(NULL, "Window Registration Failed!", "Error!",
								MB_ICONEXCLAMATION | MB_OK);
						}

						HWND hwndMain;

						// Create the main window. 
						hwndMain = CreateWindowEx(
							WS_EX_CLIENTEDGE,
							"MyWC",
							"The title of my window",
							WS_OVERLAPPEDWINDOW,
							CW_USEDEFAULT, CW_USEDEFAULT, 240, 120,
							NULL, NULL, hInstance, NULL);

						HWND hwndButton = CreateWindow(
							"BUTTON",  // Predefined class; Unicode assumed 
							"OK",      // Button text 
							WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
							10,         // x position 
							10,         // y position 
							100,        // Button width
							100,        // Button height
							hwndMain,     // Parent window
							NULL,       // No menu.
							(HINSTANCE)GetWindowLong(hwndMain, -6),
							NULL);      // Pointer not needed.

						ShowWindow(hwndMain, SW_SHOWDEFAULT);
						UpdateWindow(hwndMain);
					
						return unique_value(new custom_value<HWND>(hwndMain));
					}));
				}

				// 

				// Poll
				{
					se.declare(extended_ast::identifier({ "poll" }), extended_ast::identifier({ "_function" }));
					se.define(extended_ast::identifier({ "poll" }));
					te.set_type(extended_ast::identifier({ "poll" }), fe::types::unique_type(new function_type{
						atom_type("HWindow"),
						unset_type()
					}));
					re.set_value("poll", native_function([](unique_value from) -> unique_value {
						HWND window = dynamic_cast<custom_value<HWND>*>(from.get())->val;
						
						MSG Msg;
						if(PeekMessage(&Msg, window, 0, 0, 1))
						{
							TranslateMessage(&Msg);
							DispatchMessage(&Msg);
						}
						return unique_value(new void_value());
					}));
				}

				return { te, re, se};
			}

			static native_module* load_as_module()
			{
				auto[te, re, se] = load();
				return new native_module(module_name{ "std", "ui" }, std::move(re), std::move(te), std::move(se));
			}
		}
	}
}