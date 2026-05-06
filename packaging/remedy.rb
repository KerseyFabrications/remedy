class Remedy < Formula
  desc "Full-featured markdown pager for modern terminals"
  homepage "https://github.com/kkersey/remedy"
  # url "https://github.com/kkersey/remedy/archive/refs/tags/v0.1.0.tar.gz"
  # sha256 "UPDATE_WITH_ACTUAL_SHA256"
  version "0.3.0"
  license "GPL-3.0-or-later"

  head "https://github.com/kkersey/remedy.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "pkg-config" => :build
  depends_on "cmark-gfm"
  depends_on "ncurses"

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "make", "-C", "build", "-j#{ENV.make_jobs}"
    system "make", "-C", "build", "install"
  end

  test do
    assert_match "remedy #{version}", shell_output("#{bin}/remedy --version")
  end
end
