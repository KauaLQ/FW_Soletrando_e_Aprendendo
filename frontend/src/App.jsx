import { RouterProvider } from "react-router-dom";
import { AuthProvider } from "./context/AuthContext";
import MobileFrame from "./components/layout/MobileFrame";
import router from "./routes/router";

export default function App() {
  return (
    <AuthProvider>
      <MobileFrame>
        <RouterProvider router={router} />
      </MobileFrame>
    </AuthProvider>
  );
}