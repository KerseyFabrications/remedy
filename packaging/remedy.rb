class Remedy < Formula
  desc "Full-featured less-like markdown pager for modern terminals"
  homepage "https://github.com/KerseyFabrications/remedy"
  url "https://github.com/KerseyFabrications/remedy/archive/refs/tags/v0.4.0.tar.gz"
  sha256 "742f245ff2456e03ec1cfa886d7ad4446ec62bd23c605031d9eafe1927c52575"
  license "GPL-3.0-or-later"

  head "https://github.com/KerseyFabrications/remedy.git", branch: "master"

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
    pipe_output("#{bin}/remedy --diagnose", "# Hello World", 0)
  end
end
