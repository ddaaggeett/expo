@file:OptIn(ExperimentalCoroutinesApi::class)

package expo.modules.kotlin.jni

import com.google.common.truth.Truth
import kotlinx.coroutines.ExperimentalCoroutinesApi
import org.junit.Test

class JSIInteropModuleRegistryTest {
  @Test
  fun module_should_be_registered() = withJSIInterop(
    inlineModule {
      Name("TestModule")
      Function("syncFunction") { }
      AsyncFunction("asyncFunction") {}
    }
  ) {
    val value = evaluateScript("ExpoModules.TestModule")
    Truth.assertThat(value.isObject()).isTrue()

    val testModule = value.getObject()

    Truth.assertThat(testModule.hasProperty("syncFunction")).isTrue()
    val syncFunction = testModule.getProperty("syncFunction")
    Truth.assertThat(syncFunction.isFunction()).isTrue()

    Truth.assertThat(testModule.hasProperty("asyncFunction")).isTrue()
    val asyncFunction = testModule.getProperty("asyncFunction")
    Truth.assertThat(asyncFunction.isFunction()).isTrue()

    Truth.assertThat(
      evaluateScript("typeof ExpoModules.TestModule.syncFunction").getString()
    ).isEqualTo("function")

    Truth.assertThat(
      evaluateScript("typeof ExpoModules.TestModule.asyncFunction").getString()
    ).isEqualTo("function")
  }

  @Test
  fun sync_functions_should_be_callable() = withJSIInterop(
    inlineModule {
      Name("TestModule")
      Function("f1") { return@Function 20 }
      Function("f2") { a: String -> return@Function a + a }
    }
  ) {

    val f1Value = evaluateScript("ExpoModules.TestModule.f1()")
    val f2Value = evaluateScript("ExpoModules.TestModule.f2(\"expo\")")

    Truth.assertThat(f1Value.isNumber()).isTrue()
    val unboxedF1Value = f1Value.getDouble().toInt()
    Truth.assertThat(unboxedF1Value).isEqualTo(20)

    Truth.assertThat(f2Value.isString()).isTrue()
    val unboxedF2Value = f2Value.getString()
    Truth.assertThat(unboxedF2Value).isEqualTo("expoexpo")
  }

  @Test
  fun async_functions_should_be_callable() = withJSIInterop(
    inlineModule {
      Name("TestModule")
      AsyncFunction("f") {
        return@AsyncFunction 20
      }
    }
  ) { methodQueue ->
    val promiseResult = waitForAsyncFunction(methodQueue, "ExpoModules.TestModule.f()")
    Truth.assertThat(promiseResult.isNumber()).isTrue()
    Truth.assertThat(global().getProperty("promiseResult").getDouble().toInt()).isEqualTo(20)
  }

  @Test
  fun constants_should_be_exported() = withJSIInterop(
    inlineModule {
      Name("TestModule")
      Constants(
        "c1" to 123,
        "c2" to "string",
        "c3" to mapOf("i1" to 123)
      )
    }
  ) {

    val c1Value = evaluateScript("ExpoModules.TestModule.c1")
    val c2Value = evaluateScript("ExpoModules.TestModule.c2")
    val i1Value = evaluateScript("ExpoModules.TestModule.c3.i1")

    Truth.assertThat(c1Value.isNumber()).isTrue()
    val unboxedC1Value = c1Value.getDouble().toInt()
    Truth.assertThat(unboxedC1Value).isEqualTo(123)

    Truth.assertThat(c2Value.isString()).isTrue()
    val unboxedC2Value = c2Value.getString()
    Truth.assertThat(unboxedC2Value).isEqualTo("string")

    Truth.assertThat(i1Value.isNumber()).isTrue()
    val unboxedI1Value = i1Value.getDouble().toInt()
    Truth.assertThat(unboxedI1Value).isEqualTo(123)
  }

  @Test
  fun work() = withJSIInterop(
    inlineModule {
      Name("TestModule")
      Function("f1") { a: String, b: Int, c: Double ->
        return@Function "$a $b $c"
      }
    }
  ) {
    val result = evaluateScript("ExpoModules.TestModule.f1('expo', 123, 1.3)").getString()

    Truth.assertThat(result).isEqualTo("expo 123 1.3")
  }
}
