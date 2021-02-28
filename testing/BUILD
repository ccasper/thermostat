load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_library(
		name = "mock_impls",
		hdrs = ["mock_impls.h"],
		deps = [
				"//thermostat:core",
				],
		)

cc_test(
    name = "calculate_iaq_test",
    srcs = ["calculate_iaq_test.cc"],
    deps = [
		    "@gtest//:gtest",
		    "@gtest//:gtest_main",
		    "//thermostat:core",
		    ":mock_impls"
    ],
    copts = ["-Ithermostat"],
)

cc_test(
    name = "hvac_controller_thermostat_task_test",
    srcs = ["hvac_controller_thermostat_task_test.cc"],
    deps = [
		    "@gtest//:gtest",
		    "@gtest//:gtest_main",
		    "@google_glog//:glog",
		    "@com_github_gflags_gflags//:gflags",
		    "//thermostat:core",
		    ":mock_impls"
    ],
    copts = ["-Ithermostat"],
)

cc_test(
    name = "print_test",
    srcs = ["print_test.cc"],
    deps = [
		    "@gtest//:gtest",
		    "@gtest//:gtest_main",
		    "@google_glog//:glog",
		    "@com_github_gflags_gflags//:gflags",
		    "@com_google_absl//absl/strings",
		    "//thermostat:core",
    ],
    copts = ["-Ithermostat"],
)

cc_test(
    name = "buttons_test",
    srcs = ["buttons_test.cc"],
    deps = [
		    "@gtest//:gtest",
		    "@gtest//:gtest_main",
		    "@google_glog//:glog",
		    "@com_github_gflags_gflags//:gflags",
		    "@com_google_absl//absl/strings",
		    "//thermostat:core",
    ],
    copts = ["-Ithermostat"],
)

cc_test(
    name = "menus_test",
    srcs = ["menus_test.cc"],
    deps = [
		    "@gtest//:gtest",
		    "@gtest//:gtest_main",
		    "@google_glog//:glog",
		    "@com_github_gflags_gflags//:gflags",
		    "@com_google_absl//absl/strings",
		    "//thermostat:core",
        "mock_impls",
    ],
    copts = ["-Ithermostat"],
)

cc_test(
    name = "events_test",
    srcs = ["events_test.cc"],
    deps = [
		    "@gtest//:gtest",
		    "@gtest//:gtest_main",
		    "@google_glog//:glog",
		    "@com_github_gflags_gflags//:gflags",
		    "@com_google_absl//absl/strings",
		    "//thermostat:core",
        "mock_impls",
    ],
    copts = ["-Ithermostat"],
)

cc_test(
    name = "fan_controller_thermostat_task_test",
    srcs = ["fan_controller_thermostat_task_test.cc"],
    deps = [
                    "@gtest//:gtest",
                    "@gtest//:gtest_main",
                    "@google_glog//:glog",
                    "@com_github_gflags_gflags//:gflags",
                    "//thermostat:core",
                    ":mock_impls"
    ],
    copts = ["-Ithermostat"],
)

